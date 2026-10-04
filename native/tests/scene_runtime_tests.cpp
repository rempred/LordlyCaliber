#include "engine/rom.h"
#include "engine/scene_actors.h"
#include "engine/scene_playback.h"
#include "game/director/scene_schedule.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
using namespace lordly;
class SceneRuntimeTests : public QObject {
    Q_OBJECT
  private slots:
    void sceneTransformsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("scene-transform-mips.cjs"), path,
                                     work.filePath("transforms.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile input(work.filePath("transforms.json"));
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto oracle = QJsonDocument::fromJson(input.readAll()).object();
            const auto rom = Rom::open(path);
            QMap<int, SceneTransformAsset> assets;
            for (const auto entry : oracle["assets"].toArray()) {
                const auto item = entry.toObject();
                const int selector = item["selector"].toInt();
                if (item["decoded"].isNull()) {
                    QVERIFY_THROWS_EXCEPTION(Error, SceneTransformAsset::load(rom, selector));
                    continue;
                }
                const auto asset = SceneTransformAsset::load(rom, selector);
                QCOMPARE(asset.key, quint32(item["key"].toInteger()));
                QCOMPARE(asset.decoded, QByteArray::fromHex(item["decoded"].toString().toLatin1()));
                assets[selector] = asset;
            }
            QCOMPARE(oracle["assets"].toArray().size(), 62);
            QCOMPARE(assets.size(), 61);
            QCOMPARE(oracle["rejected"].toArray().size(), 3);
            for (const auto entry : oracle["rejected"].toArray()) {
                const auto row = entry.toObject();
                SceneTransforms state;
                QVERIFY_THROWS_EXCEPTION(
                    Error, state.start(assets[row["selector"].toInt()], row["group"].toInt(), 3));
                QVERIFY(state.done());
            }
            auto records = [](const SceneTransforms &state) {
                QByteArray bytes;
                for (int channel = 0; channel < 20; ++channel)
                    bytes += state.channel(channel);
                return bytes;
            };
            QCOMPARE(records(SceneTransforms{}),
                     QByteArray::fromHex(oracle["identity"].toString().toLatin1()));
            int snapshots = 0;
            const auto results = oracle["results"].toArray();
            for (const auto value : results) {
                const auto row = value.toObject();
                const auto asset = row["changed"].isNull()
                                       ? assets[row["selector"].toInt()]
                                       : SceneTransformAsset::parse(QByteArray::fromHex(
                                             row["changed"].toString().toLatin1()));
                SceneTransforms state;
                state.start(asset, row["group"].toInt(), row["duration"].toInt());
                const auto expected = row["states"].toArray();
                for (int i = 0; i < expected.size(); ++i) {
                    if (i)
                        state.advance();
                    const auto snapshot = expected[i].toObject();
                    QCOMPARE(records(state),
                             QByteArray::fromHex(snapshot["channels"].toString().toLatin1()));
                    QCOMPARE(state.done(), snapshot["done"].toBool());
                    QCOMPARE(state.remaining(), quint32(snapshot["remaining"].toInteger()));
                    QCOMPARE(state.nextKeyframe(), snapshot["next"].toInt());
                    QCOMPARE(state.deltas(),
                             QByteArray::fromHex(snapshot["deltas"].toString().toLatin1()));
                    ++snapshots;
                }
            }
            qInfo() << "Transform comparison:" << assets.size() << "resources," << results.size()
                    << "sequences," << snapshots << "snapshots";
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneTransformInvalidInputsAreAtomic() {
        QByteArray bytes(8 + 960 + 16, 0);
        put32(bytes, 0, 2);
        put32(bytes, 4, 1);
        put32(bytes, 488, 12345);
        put32(bytes, 976, 1);
        put32(bytes, 980, 0xffffffffU);
        const auto asset = SceneTransformAsset::parse(bytes);
        SceneTransforms state;
        state.start(asset, 0, 3);
        state.advance();
        const auto before = state.channel(0), deltas = state.deltas();
        const auto remaining = state.remaining();
        QVERIFY_THROWS_EXCEPTION(Error, state.start(asset, 0, 0));
        QCOMPARE(state.channel(0), before);
        QCOMPARE(state.deltas(), deltas);
        QCOMPARE(state.remaining(), remaining);
        QVERIFY_THROWS_EXCEPTION(Error, state.start(asset, 1, 1));
        auto malformed = bytes;
        put32(malformed, 976, 2);
        QVERIFY_THROWS_EXCEPTION(Error, state.start(SceneTransformAsset::parse(malformed), 0, 3));
        QVERIFY_THROWS_EXCEPTION(Error, SceneTransformAsset::parse(bytes.first(bytes.size() - 4)));
        auto overflow = bytes;
        put32(overflow, 488, 0x80000000U);
        QVERIFY_THROWS_EXCEPTION(Error, state.start(SceneTransformAsset::parse(overflow), 0, -1));
        QCOMPARE(state.channel(0), before);
        QCOMPARE(state.deltas(), deltas);
        QCOMPARE(state.remaining(), remaining);
        auto oversized = asset;
        oversized.groups[0] = QVector<int>(21, 0);
        QVERIFY_THROWS_EXCEPTION(Error, state.start(oversized, 0, 1));
        QCOMPARE(state.channel(0), before);
        QVERIFY_THROWS_EXCEPTION(Error, state.supplyChannel(20, QByteArray(88, 0)));
        auto invalid = before;
        put32(invalid, 64, 0x7f800000);
        QVERIFY_THROWS_EXCEPTION(Error, state.supplyChannel(0, invalid));
        QCOMPARE(state.channel(0), before);
    }
    void sceneTransformPlaybackTiming() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto bytes = Rom::open(path).bytes();
        int allocation = 0x2700000;
        auto resource = [&](const QByteArray &raw) {
            const auto packed = encodeLzss(raw);
            const auto key = quint32(allocation - 0x594280);
            put32(bytes, allocation, quint32(packed.size() + 4));
            put32(bytes, allocation + 4, quint32(raw.size()));
            bytes.replace(allocation + 8, packed.size(), packed);
            allocation += (int(packed.size()) + 9) & ~1;
            return key;
        };
        QByteArray transform(8 + 960 + 16, 0);
        put32(transform, 0, 2);
        put32(transform, 4, 1);
        put32(transform, 8 + 0xa0, 2000);
        put32(transform, 488 + 0xa0, 8000);
        put32(transform, 976, 1);
        put32(transform, 980, 0xffffffffU);
        put32(bytes, 0x594280 + 0x19a63ec + 4, resource(transform));
        const QVector<quint32> words{8,    0, 0,           3,          0,           0x80000000U,
                                     0xd,  0, 1,           0,          0x80000002U, 0x80000000U,
                                     0x7d, 0, 0x80000001U, 0xff000001U};
        QByteArray script(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(script, i * 4, words[i]);
        put32(bytes, 0x1f3ca84, 4);
        put32(bytes, 0x1f3ca88, resource(script));
        const auto rom = Rom::fromBytes(bytes);
        const auto directors = Directors::load(rom);
        auto create = [&] {
            ScenePlaybackStart start;
            start.mode = 0;
            return ScenePlayback(rom, directors, 0, start, SceneCameras::freshModeTwo(rom));
        };
        // This scene contains no other scheduled domain objects.
        auto emptyDomains = [](LordlySceneService) { return true; };
        auto baseline = create(), resumed = create();
        const float positions[] = {2, 4, 6, 8, 8};
        bool retry = false;
        auto delayedDomains = [&](LordlySceneService) {
            retry = !retry;
            return !retry;
        };
        for (int tick = 0; tick < 5; ++tick) {
            const auto expected = tick == 4 ? ScenePlayback::Released : ScenePlayback::Advanced;
            const auto result = baseline.advance(emptyDomains);
            QVERIFY2(result == expected, qPrintable(baseline.reason()));
            QCOMPARE(std::bit_cast<float>(u32(baseline.transforms().channel(0), 0x40)),
                     positions[tick]);
            QCOMPARE(baseline.transforms().remaining(), quint32(qMax(3 - tick, 0)));
            QCOMPARE(baseline.transforms().done(), tick >= 3);
            ScenePlayback::Result continued;
            int attempts = 0;
            do {
                continued = resumed.advance(delayedDomains, {}, 1, 1);
                QVERIFY2(++attempts < 200, qPrintable(resumed.reason()));
            } while (continued == ScenePlayback::Suspended || continued == ScenePlayback::Budget);
            QCOMPARE(continued, expected);
            QCOMPARE(resumed.transforms().channel(0), baseline.transforms().channel(0));
            QCOMPARE(resumed.transforms().remaining(), baseline.transforms().remaining());
            QCOMPARE(resumed.director().state().resume_cursor,
                     baseline.director().state().resume_cursor);
        }
        auto alternate = create();
        QCOMPARE(alternate.advance(emptyDomains), ScenePlayback::Advanced);
        alternate.setAlternate(1);
        const auto held = alternate.transforms().channel(0);
        QCOMPARE(alternate.advance(), ScenePlayback::Advanced);
        QCOMPARE(alternate.transforms().channel(0), held);
        QCOMPARE(alternate.transforms().remaining(), 3U);
        alternate.setAlternate(0);
        QCOMPARE(alternate.advance(emptyDomains), ScenePlayback::Advanced);
        QCOMPARE(std::bit_cast<float>(u32(alternate.transforms().channel(0), 0x40)), 4.f);
    }
    void actorDividerAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("actor-divider-mips.cjs"), path,
                                     work.filePath("divider.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile input(work.filePath("divider.json"));
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto rows =
                QJsonDocument::fromJson(input.readAll()).object()["results"].toArray();
            QCOMPARE(rows.size(), 72);
            const auto rom = Rom::open(path);
            for (const auto value : rows) {
                const auto row = value.toObject();
                SceneActors actors(rom, 0, 0);
                ActorDivider divider;
                divider.supply(QByteArray::fromHex(row["initialDivider"].toString().toLatin1()));
                actors.supplyDivider(divider);
                for (const auto entry : row["initial"].toArray()) {
                    const auto item = entry.toObject();
                    actors.supplyActor(item["slot"].toInt(),
                                       QByteArray::fromHex(item["record"].toString().toLatin1()));
                }
                const auto operations = row["operations"].toArray(),
                           states = row["states"].toArray();
                for (int i = 0; i < operations.size(); ++i) {
                    const auto op = operations[i].toObject(), state = states[i].toObject();
                    if (op.contains("positions")) {
                        for (const auto p : op["positions"].toArray()) {
                            const auto item = p.toObject();
                            const int slot = item["slot"].toInt();
                            auto record = actors.actor(slot)->record();
                            put32(record, 0x11c,
                                  std::bit_cast<quint32>(float(item["x"].toDouble())));
                            put32(record, 0x124,
                                  std::bit_cast<quint32>(float(item["z"].toDouble())));
                            actors.supplyActor(slot, record);
                        }
                        actors.advanceDivider();
                    } else {
                        QVector<quint32> args;
                        for (const auto arg : op["args"].toArray())
                            args << quint32(arg.toInteger());
                        int ignored = 0;
                        QVERIFY(actors.service(quint32(op["op"].toInt()), args, ignored));
                    }
                    QCOMPARE(actors.divider().record(),
                             QByteArray::fromHex(state["divider"].toString().toLatin1()));
                    for (const auto entry : state["actors"].toArray()) {
                        const auto item = entry.toObject();
                        QCOMPARE(actors.actor(item["slot"].toInt())->record(),
                                 QByteArray::fromHex(item["record"].toString().toLatin1()));
                    }
                }
            }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void actorDividerPrecedesMovement() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto resources = Directors::load(rom);
        int selector = 0;
        while (!resources.selectors()[selector])
            ++selector;
        ScenePlaybackStart start;
        start.mode = 0;
        ScenePlayback scene(rom, std::move(resources), selector, start,
                            SceneCameras::freshModeTwo(rom));
        QByteArray record(336, 0);
        put32(record, 0xe8, 30);
        put32(record, 0x11c, std::bit_cast<quint32>(-1.f));
        scene.actors().supplyActor(0, record);
        int ignored = 0;
        QVERIFY(scene.actors().service(0x5f, {0, 0, 10, 0, 3, 1}, ignored));
        LordlyActorMotion motion{};
        motion.present = 1;
        motion.remaining = 2;
        motion.vx = 2;
        scene.actors().supplyMotion(0, motion);
        auto emptyOtherDomains = [](LordlySceneService) { return true; };
        int attempts = 0;
        while (scene.schedule().phase != 12) {
            QCOMPARE(scene.advance(emptyOtherDomains, {}, 100, 1), ScenePlayback::Budget);
            QVERIFY(++attempts < 20);
        }
        QCOMPARE(u8(scene.actors().actor(0)->record(), 0x13e), quint8(1));
        QCOMPARE(scene.actors().actor(0)->pose().actor().position.x, -1.f);
        QCOMPARE(scene.advance(emptyOtherDomains, {}, 100, 1), ScenePlayback::Budget);
        QCOMPARE(scene.actors().actor(0)->pose().actor().position.x, 1.f);
        QCOMPARE(u8(scene.actors().actor(0)->record(), 0x13e), quint8(1));
        scene.actors().advanceDivider();
        QCOMPARE(u8(scene.actors().actor(0)->record(), 0x13e), quint8(3));
        QVERIFY(scene.actors().service(0x75, {}, ignored));
        QCOMPARE(u8(scene.actors().actor(0)->record(), 0x13e), quint8(3));
        const auto before = scene.actors().actor(0)->record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.actors().service(0x1c, {28, 1}, ignored));
        QCOMPARE(scene.actors().actor(0)->record(), before);
    }
    void actorMaterialsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("actor-material-mips.cjs"), path,
                                     work.filePath("materials.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile input(work.filePath("materials.json"));
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto results =
                QJsonDocument::fromJson(input.readAll()).object()["results"].toArray();
            QCOMPARE(results.size(), 200);
            const auto rom = Rom::open(path);
            for (const auto value : results) {
                const auto row = value.toObject();
                SceneActors actors(rom, 2, 0);
                for (const auto entry : row["initial"].toArray()) {
                    const auto item = entry.toObject();
                    actors.supplyActor(item["slot"].toInt(),
                                       QByteArray::fromHex(item["record"].toString().toLatin1()));
                }
                const auto operations = row["operations"].toArray(),
                           states = row["states"].toArray();
                for (int i = 0; i < operations.size(); ++i) {
                    const auto op = operations[i].toObject(), state = states[i].toObject();
                    QByteArray commands;
                    if (op.contains("draw")) {
                        for (const auto slot : op["draw"].toArray())
                            commands += actors.renderMaterials(slot.toInt());
                    } else {
                        QVector<quint32> args;
                        for (const auto arg : op["args"].toArray())
                            args << quint32(arg.toInteger());
                        int result = 0;
                        QVERIFY(actors.service(quint32(op["op"].toInt()), args, result));
                    }
                    QCOMPARE(commands,
                             QByteArray::fromHex(state["commands"].toString().toLatin1()));
                    for (const auto entry : state["actors"].toArray()) {
                        const auto item = entry.toObject();
                        QCOMPARE(actors.actor(item["slot"].toInt())->record(),
                                 QByteArray::fromHex(item["record"].toString().toLatin1()));
                    }
                    QMap<int, QByteArray> tints, materials;
                    for (const auto entry : state["tints"].toArray()) {
                        const auto item = entry.toObject();
                        tints[item["slot"].toInt()] =
                            QByteArray::fromHex(item["record"].toString().toLatin1());
                    }
                    for (const auto entry : state["materials"].toArray()) {
                        const auto item = entry.toObject();
                        materials[item["slot"].toInt()] =
                            QByteArray::fromHex(item["record"].toString().toLatin1());
                    }
                    for (int slot = 0; slot < 28; ++slot) {
                        QCOMPARE(actors.materials().tint(slot), tints.value(slot));
                        QCOMPARE(actors.materials().material(slot), materials.value(slot));
                        int result = -1;
                        QVERIFY(actors.service(0x1f, {0, 0, quint32(slot)}, result));
                        QCOMPARE(bool(result), state["active"].toArray().contains(slot));
                    }
                }
            }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void actorMaterialOwnership() {
        ActorMaterials materials;
        materials.startTint({3, 1, 2, 3, 4, 5, 6, 10, 1}, {});
        const auto prior = materials.tint(3);
        QVERIFY_THROWS_EXCEPTION(Error,
                                 materials.startTint({0xffffffffU, 9, 8, 7, 6, 5, 4, 2, 0}, {0}));
        QCOMPARE(materials.tint(3), prior);
        QVERIFY(materials.tint(0).isEmpty());
        QVERIFY_THROWS_EXCEPTION(Error, materials.startMaterial({28, 1, 2, 3, 4, 5, 6, 0}));
        QVERIFY_THROWS_EXCEPTION(Error, materials.releaseMaterial(65536));
        QVERIFY_THROWS_EXCEPTION(Error, materials.active(-1));
        auto invalid = prior;
        put16(invalid, 8, 0);
        put16(invalid, 10, 65535);
        materials.supplyTint(3, invalid);
        QByteArray actor(336, char(0xa7));
        put32(actor, 0xe4, 3);
        const auto actorBefore = actor;
        QVERIFY_THROWS_EXCEPTION(Error, materials.render(actor));
        QCOMPARE(actor, actorBefore);
        QCOMPARE(materials.tint(3), invalid);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            return;
        SceneActors actors(Rom::open(path), 2, 0);
        int result = 0;
        QVERIFY_THROWS_EXCEPTION(Error, actors.service(0xa7, {0xffffffffU}, result));
        QVERIFY_THROWS_EXCEPTION(Error, actors.renderMaterials(0));
    }
    void poseAudioSurvivesBudget() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        for (int bank = 1; bank <= ActorArt::bankCount(rom); ++bank) {
            const auto art = ActorArt::load(rom, bank);
            for (int state = 0; state < art.poses().size(); ++state) {
                const auto &program = art.poses()[state].records;
                if (program.isEmpty() || program.first().opcode < 17 || program.first().opcode > 20)
                    continue;
                ActorPosePlayer uninterrupted(rom, art, state), resumed(rom, art, state);
                QCOMPARE(uninterrupted.advance(), LORDLY_POSE_OK);
                QCOMPARE(resumed.advance(1), LORDLY_POSE_BUDGET);
                QVERIFY(!resumed.audioRequests().isEmpty());
                int attempts = 0, status;
                do {
                    status = resumed.advance(1);
                    QVERIFY(++attempts < 100);
                } while (status == LORDLY_POSE_BUDGET);
                QCOMPARE(status, LORDLY_POSE_OK);
                QCOMPARE(resumed.audioRequests().size(), uninterrupted.audioRequests().size());
                for (int i = 0; i < resumed.audioRequests().size(); ++i) {
                    QCOMPARE(resumed.audioRequests()[i].context,
                             uninterrupted.audioRequests()[i].context);
                    QCOMPARE(resumed.audioRequests()[i].value,
                             uninterrupted.audioRequests()[i].value);
                }
                return;
            }
        }
        QFAIL("The retail corpus should contain a program beginning with sound");
    }
    void actorSweepBudgetResume() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        SceneActors baseline(rom, 2, 0), resumed(rom, 2, 0);
        const int actorSlots[] = {0, 7, 27};
        const int banks[] = {30, 57, 63};
        for (int i = 0; i < 3; ++i) {
            int result = 0;
            const QVector<quint32> args{
                quint32(actorSlots[i]), quint32(banks[i]), 0, 0, 1000, 2000, 3000, 0, 0};
            QVERIFY(baseline.service(0x14, args, result));
            QVERIFY(resumed.service(0x14, args, result));
        }
        int pauses = 0;
        for (int tick = 0; tick < 64; ++tick) {
            QVERIFY(baseline.advancePoses());
            int attempts = 0;
            while (!resumed.advancePoses(1)) {
                ++pauses;
                QVERIFY(++attempts < 100);
            }
            for (const int slot : actorSlots)
                QCOMPARE(resumed.actor(slot)->record(), baseline.actor(slot)->record());
        }
        QVERIFY(pauses > 0);
    }
    void effectsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("scene-effects-mips.cjs"),
                                 path, work.filePath("effects.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("effects.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        const auto cases = data["results"].toArray();
        QCOMPARE(cases.size(), 88);
        for (const auto &v : cases) {
            const auto row = v.toObject();
            SceneEffects effects;
            effects.setSharedColor({0x11, 0x22, 0x33, 0x44, 0x55});
            for (const auto &a : row["initial"].toArray()) {
                const auto record = a.toObject();
                effects.supplyColor(record["slot"].toInt(),
                                    QByteArray::fromHex(record["record"].toString().toLatin1()));
            }
            const auto operations = row["operations"].toArray(), states = row["states"].toArray();
            for (int step = 0; step < operations.size(); ++step) {
                const auto op = operations[step].toObject();
                if (op.contains("args")) {
                    QVector<quint32> args;
                    for (const auto &a : op["args"].toArray())
                        args << quint32(a.toInt());
                    QVERIFY(effects.service(0x33, args));
                } else
                    effects.advanceColors();
                QJsonArray actual;
                for (int slot = 0; slot < 20; ++slot)
                    if (!effects.colorRecord(slot).isEmpty())
                        actual.append(QJsonObject{
                            {"slot", slot},
                            {"record", QString::fromLatin1(effects.colorRecord(slot).toHex())}});
                QCOMPARE(actual, states[step].toObject()["records"].toArray());
                const auto &shared = effects.sharedColor();
                QCOMPARE(QByteArray(reinterpret_cast<const char *>(shared.data()), 5).toHex(),
                         states[step].toObject()["shared"].toString().toLatin1());
            }
        }
        SceneEffects effects;
        QCOMPARE(effects.specialBlackAlpha(), 0);
        QCOMPARE(data["special"].toArray().size(), 70);
        for (const auto &v : data["special"].toArray()) {
            const auto row = v.toObject();
            if (row["restart"].toBool())
                QVERIFY(effects.service(0xa9, {}));
            else
                effects.advanceSpecialBlack();
            QCOMPARE(effects.specialBlackRecord().toHex(), row["record"].toString().toLatin1());
            QCOMPARE(effects.specialBlackAlpha(), row["alpha"].toInt());
        }
        for (const auto &v : data["sources"].toArray()) {
            const auto row = v.toObject();
            QCOMPARE(SceneEffects::visibleAboveSpecialBlack(row["source"].toInt()),
                     row["visible"].toBool());
        }
        QVERIFY(effects.service(0x33, {1, 2, 3, 4, 5, 6, 3, 0}));
        const auto before = effects.colorRecord(0);
        auto invalid = before;
        put16(invalid, 0, 0);
        put16(invalid, 2, 0xffff);
        effects.supplyColor(19, invalid);
        QVERIFY_THROWS_EXCEPTION(Error, effects.advanceColors());
        QCOMPARE(effects.colorRecord(0), before);
        QCOMPARE(effects.colorRecord(19), invalid);
        QVERIFY_THROWS_EXCEPTION(Error, effects.service(0x33, {1, 2, 3, 4, 5, 6, 3, 20}));
        QCOMPARE(effects.colorRecord(0), before);
    }
    void turnsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-turn-mips.cjs"),
             path, work.filePath("turns.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("turns.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 266);
        const auto rom = Rom::open(path);
        for (const auto &v : cases) {
            const auto row = v.toObject();
            SceneActors scene(rom, 2, 0);
            scene.supplyActor(3, QByteArray::fromHex(row["initial"].toString().toLatin1()));
            scene.supplyTurnJob(
                3, ActorTurnJob(QByteArray::fromHex(row["initialJob"].toString().toLatin1())));
            const auto operations = row["operations"].toArray(), states = row["states"].toArray();
            for (int i = 0; i < operations.size(); ++i) {
                const auto operation = operations[i].toObject();
                int result = 0;
                if (operation.contains("args")) {
                    QVector<quint32> args;
                    for (const auto &a : operation["args"].toArray())
                        args << quint32(a.toInt());
                    QVERIFY(scene.service(0x15, args, result));
                } else
                    scene.advanceTurns();
                const auto expected = states[i].toObject();
                QCOMPARE(scene.actor(3)->record().toHex(), expected["actor"].toString().toLatin1());
                QCOMPARE(scene.turnJob(3).record().toHex(), expected["job"].toString().toLatin1());
                QVERIFY(scene.service(0x16, {0, 0, 3}, result));
                QCOMPARE(result, expected["query"].toInt());
            }
        }
        SceneActors missing(rom, 2, 0);
        int result = 0;
        QVERIFY(missing.service(0x15, {0, 0, 1, 0xffffffffU, 12}, result));
        const auto before = missing.turnJob(0).record();
        QVERIFY_THROWS_EXCEPTION(Error, missing.advanceTurns());
        QCOMPARE(missing.turnJob(0).record(), before);
        QVERIFY_THROWS_EXCEPTION(Error, missing.service(0x15, {0, 1, 1, 1, 12}, result));
        QCOMPARE(missing.turnJob(0).record(), before);
        QVERIFY_THROWS_EXCEPTION(Error, missing.service(0x15, {0, 0, 1, 0, 12}, result));
        QCOMPARE(missing.turnJob(0).record(), before);
    }
    void movementAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("scene-movement-mips.cjs"),
                                 path, work.filePath("movement.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("movement.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 234);
        const auto rom = Rom::open(path);
        auto fromMotion = [](const QByteArray &b, bool present) {
            return LordlyActorMotion{std::bit_cast<float>(u32(b, 0)),
                                     std::bit_cast<float>(u32(b, 4)),
                                     std::bit_cast<float>(u32(b, 8)),
                                     u16(b, 12),
                                     u8(b, 14),
                                     u8(b, 15),
                                     present ? 1 : 0};
        };
        auto motionBytes = [](const LordlyActorMotion &m) {
            QByteArray b(16, 0);
            put32(b, 0, std::bit_cast<quint32>(m.vx));
            put32(b, 4, std::bit_cast<quint32>(m.vy));
            put32(b, 8, std::bit_cast<quint32>(m.vz));
            put16(b, 12, m.remaining);
            put8(b, 14, m.pause);
            put8(b, 15, m.retained);
            return b;
        };
        for (const auto &v : cases) {
            const auto row = v.toObject(), options = row["options"].toObject();
            QJsonArray terrain;
            SceneActors scene(rom, options["mode"].toInt(), 0, [&](int mode, float x, float z) {
                terrain.append(QJsonArray{mode, x, z});
                return 7.25f + float(terrain.size()) / 4;
            });
            scene.supplySceneFlags(quint32(options["stateFlags"].toInt()));
            for (const auto &a : row["actors"].toArray()) {
                const auto actor = a.toObject();
                scene.supplyActor(actor["slot"].toInt(),
                                  QByteArray::fromHex(actor["record"].toString().toLatin1()));
            }
            for (const auto &a : row["motions"].toArray()) {
                const auto motion = a.toObject();
                scene.supplyMotion(
                    motion["slot"].toInt(),
                    fromMotion(QByteArray::fromHex(motion["record"].toString().toLatin1()),
                               motion["present"].toBool()));
            }
            for (const auto &a : row["depths"].toArray()) {
                const auto depth = a.toObject();
                scene.supplyDepthAttachment(
                    depth["slot"].toInt(),
                    QByteArray::fromHex(depth["record"].toString().toLatin1()));
            }
            for (const auto &s : row["states"].toArray()) {
                scene.advanceMovement();
                const auto state = s.toObject();
                for (const auto &a : state["actors"].toArray()) {
                    const auto actor = a.toObject();
                    QCOMPARE(scene.actor(actor["slot"].toInt())->record().toHex(),
                             actor["record"].toString().toLatin1());
                }
                for (const auto &a : state["motions"].toArray()) {
                    const auto motion = a.toObject();
                    const auto &actual = scene.motion(motion["slot"].toInt());
                    QCOMPARE(motionBytes(actual).toHex(), motion["record"].toString().toLatin1());
                    QCOMPARE(bool(actual.present), motion["present"].toBool());
                }
                QJsonArray depths;
                for (int slot = 0; slot < 28; ++slot)
                    if (!scene.depthAttachment(slot).isEmpty())
                        depths.append(QJsonObject{
                            {"slot", slot},
                            {"record", QString::fromLatin1(scene.depthAttachment(slot).toHex())}});
                QCOMPARE(depths, state["depths"].toArray());
                QCOMPARE(terrain, state["terrain"].toArray());
            }
        }
        const auto initial = QByteArray::fromHex(
            cases[0].toObject()["actors"].toArray()[0].toObject()["record"].toString().toLatin1());
        SceneActors missingFlags(rom, 2, 0);
        missingFlags.supplyActor(0, initial);
        missingFlags.supplyMotion(0, {1, 0, 2, 3, 0, 0, 1});
        QVERIFY_THROWS_EXCEPTION(Error, missingFlags.advanceMovement());
        QCOMPARE(missingFlags.actor(0)->record(), initial);
        QCOMPARE(missingFlags.motion(0).remaining, 3);
        missingFlags.supplySceneFlags(0);
        auto ground = initial;
        put8(ground, 0x145, 1);
        missingFlags.supplyActor(27, ground);
        missingFlags.supplyMotion(27, {1, 0, 2, 3, 0, 0, 1});
        QVERIFY_THROWS_EXCEPTION(Error, missingFlags.advanceMovement());
        QCOMPARE(missingFlags.actor(0)->record(), initial);
        QCOMPARE(missingFlags.motion(0).remaining, 3);
        QCOMPARE(missingFlags.actor(27)->record(), ground);
    }
    void playbackCompositionAndSuspension() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto bytes = Rom::open(path).bytes();
        int allocation = 0x2700000;
        auto program = [&](const QVector<quint32> &words) {
            QByteArray raw(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(raw, i * 4, words[i]);
            const auto compressed = encodeLzss(raw);
            const auto key = quint32(allocation - 0x594280);
            put32(bytes, allocation, quint32(compressed.size() + 4));
            put32(bytes, allocation + 4, quint32(raw.size()));
            bytes.replace(allocation + 8, compressed.size(), compressed);
            allocation += (int(compressed.size()) + 9) & ~1;
            return key;
        };
        const QVector<quint32> setup{
            0x46, 0, 57,  0,    0,     13,    17,   1,    100, 0,    0x46, 29,   63, 0, 0,    40,
            15,   3, 100, 0,    0x14,  0,     30,   1,    2,   1000, 2000, 3000, 0,  0, 0x7b, 0,
            1,    5, 3,   0x2c, 10000, 20000, 1000, 1000, 2,   0x1b, 3,    5,    6,  7, 255,  0};
        auto ordinary = setup;
        ordinary << 1 << 0 << 0x80000000U << 0xe << 2 << 2 << 0 << 0x80000002U << 0x80000000U
                 << 0x7d << 2 << 0x80000001U << 0xff000001U;
        auto alternate = setup;
        alternate << 0x97 << 0 << 0x80000000U << 0x39 << 1 << 0 << 0x80000000U << 0x7d
                  << 0x80000001U << 0xff000001U;
        put32(bytes, 0x1f3ca84, 8);
        put32(bytes, 0x1f3ca88, program(ordinary));
        put32(bytes, 0x1f3ca8c, program(alternate));
        const auto rom = Rom::fromBytes(bytes);
        const auto resources = Directors::load(rom);
        auto create = [&](int selector) {
            return ScenePlayback(rom, resources, selector, {}, SceneCameras::freshModeTwo(rom));
        };
        // This fixture explicitly has no terrain motion, channels or
        // other scheduled objects. Their callbacks acknowledge those empty pools.
        auto emptyDomains = [](LordlySceneService) { return true; };
        auto baseline = create(0);
        QCOMPARE(baseline.advance(), ScenePlayback::Suspended);
        QCOMPARE(baseline.pendingService(), std::optional{LORDLY_SCENE_AFTER_DIRECTOR});
        QCOMPARE(baseline.completedUpdates(), 0U);
        QVector<QByteArray> actors, projections, fades, sprites, lastSprites;
        QVector<int> cursors;
        const float heights[] = {2, 7, 12, 12};
        const int counts[] = {2, 1, 0, 0};
        for (int tick = 0; tick < 4; ++tick) {
            const auto result = baseline.advance(emptyDomains);
            QVERIFY2(result == (tick == 3 ? ScenePlayback::Released : ScenePlayback::Advanced),
                     qPrintable(baseline.reason()));
            QCOMPARE(baseline.completedUpdates(), quint64(tick + 1));
            QCOMPARE(baseline.director().state().counter, quint32(tick + 1));
            QCOMPARE(baseline.actors().actor(0)->pose().actor().position.y, heights[tick]);
            QCOMPARE(baseline.projection().remaining(), counts[tick]);
            QCOMPARE(baseline.fade().rgba()[3], 255); // Separate scheduler slot.
            actors << baseline.actors().actor(0)->record();
            projections << baseline.projection().record();
            fades << baseline.fade().record();
            sprites << baseline.sprites().sprite(0)->record();
            lastSprites << baseline.sprites().sprite(29)->record();
            cursors << baseline.director().state().resume_cursor;
        }
        QCOMPARE(baseline.advance(emptyDomains), ScenePlayback::Released);
        QCOMPARE(baseline.completedUpdates(), 4U);
        baseline.advanceFade();
        QVERIFY(!baseline.fade().present());
        auto resumed = create(0);
        bool retry = false;
        auto delayedDomains = [&](LordlySceneService) {
            retry = !retry;
            return !retry;
        };
        for (int tick = 0; tick < 4; ++tick) {
            ScenePlayback::Result result;
            int attempts = 0;
            do {
                result = resumed.advance(delayedDomains, {}, 1, 1);
                QVERIFY(++attempts < 200);
            } while (result == ScenePlayback::Suspended || result == ScenePlayback::Budget);
            QCOMPARE(result, tick == 3 ? ScenePlayback::Released : ScenePlayback::Advanced);
            QCOMPARE(resumed.actors().actor(0)->record(), actors[tick]);
            QCOMPARE(resumed.projection().record(), projections[tick]);
            QCOMPARE(resumed.fade().record(), fades[tick]);
            QCOMPARE(resumed.sprites().sprite(0)->record(), sprites[tick]);
            QCOMPARE(resumed.sprites().sprite(29)->record(), lastSprites[tick]);
            QCOMPARE(resumed.director().state().resume_cursor, cursors[tick]);
            QCOMPARE(resumed.director().state().counter, quint32(tick + 1));
            QCOMPARE(resumed.completedUpdates(), quint64(tick + 1));
        }
        auto switched = create(1);
        QCOMPARE(switched.advance(emptyDomains), ScenePlayback::Advanced);
        QCOMPARE(switched.schedule().alternate, 1);
        const auto heldSprite = switched.sprites().sprite(0)->record();
        QCOMPARE(switched.advance(), ScenePlayback::Advanced); // Alternate needs no other domains.
        QCOMPARE(switched.schedule().alternate, 0);
        QCOMPARE(switched.sprites().sprite(0)->record(), heldSprite);
        QCOMPARE(switched.actors().actor(0)->pose().actor().position.y, 2.f);
        QCOMPARE(switched.projection().remaining(), 2);
        QCOMPARE(switched.advance(emptyDomains), ScenePlayback::Released);
        QCOMPARE(switched.actors().actor(0)->pose().actor().position.y, 7.f);
        QCOMPARE(switched.projection().remaining(), 1);
    }
    void heightAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-height-mips.cjs"),
             path, work.filePath("height.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("height.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 247);
        const auto rom = Rom::open(path);
        for (const auto &v : cases) {
            const auto row = v.toObject();
            const int slot = row["slot"].toInt();
            SceneActors scene(rom, 2, 0);
            scene.supplyActor(slot, QByteArray::fromHex(row["initialActor"].toString().toLatin1()));
            scene.supplyHeightJob(
                slot, ActorHeightJob(QByteArray::fromHex(row["initial"].toString().toLatin1())));
            if (row["args"].isArray()) {
                QVector<quint32> args{quint32(slot)};
                for (const auto &a : row["args"].toArray())
                    args << quint32(a.toInt());
                int ignored = 0;
                QVERIFY(scene.service(0x7b, args, ignored));
            }
            const auto states = row["states"].toArray();
            for (int step = 0; step < states.size(); ++step) {
                if (step)
                    scene.advanceHeights();
                const auto expected = states[step].toObject();
                QCOMPARE(scene.actor(slot)->record().toHex(),
                         expected["actor"].toString().toLatin1());
                QCOMPARE(scene.heightJob(slot).record().toHex(),
                         expected["job"].toString().toLatin1());
            }
        }
        // Failure in a later slot must not commit an earlier actor's update.
        SceneActors scene(rom, 2, 0);
        const auto initial =
            QByteArray::fromHex(cases[0].toObject()["initialActor"].toString().toLatin1());
        scene.supplyActor(0, initial);
        int ignored = 0;
        QVERIFY(scene.service(0x7b, {0, 1, 5, 4}, ignored));
        QVERIFY(scene.service(0x7b, {27, 1, 5, 4}, ignored));
        const auto job = scene.heightJob(0).record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.advanceHeights());
        QCOMPARE(scene.actor(0)->record(), initial);
        QCOMPARE(scene.heightJob(0).record(), job);
        QVERIFY_THROWS_EXCEPTION(Error, scene.service(0x7b, {0, 0xffffffffU, 100, 0}, ignored));
        QCOMPARE(scene.heightJob(0).record(), job);
        QVERIFY(scene.service(0x7b, {0, 0, 5, 4}, ignored));
        const auto invalid = scene.heightJob(0).record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.advanceHeights());
        QCOMPARE(scene.heightJob(0).record(), invalid);
    }
    void schedulerAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("scene-schedule-mips.cjs"),
                                 path, work.filePath("schedule.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("schedule.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        const auto cases = data["results"].toArray();
        QCOMPARE(cases.size(), 265);
        DirectorSession counter(Directors::load(Rom::open(path)), 2, 2);
        for (const auto &v : data["counterCases"].toArray()) {
            const auto row = v.toObject();
            counter.setCounter(quint32(row["before"].toInteger()));
            counter.advanceCounter();
            QCOMPARE(counter.state().counter, quint32(row["after"].toInteger()));
        }
        for (const auto &v : cases) {
            const auto row = v.toObject();
            // Repeat with a one-service host budget and a pause before every
            // service. Both must preserve native order and all state snapshots.
            for (const bool interrupted : {false, true}) {
                struct Context {
                    LordlySceneSchedule state{};
                    QJsonObject change;
                    QJsonArray calls;
                    bool interrupted = false, retry = false;
                } context;
                context.state.mode = quint8(row["mode"].toInt());
                context.state.alternate = quint8(row["alternate"].toInt());
                context.state.delayed_counter = quint8(row["counter"].toInt());
                context.change = row["change"].toObject();
                context.interrupted = interrupted;
                auto invoke = [](void *opaque, LordlySceneService service) -> int {
                    auto &c = *static_cast<Context *>(opaque);
                    if (c.interrupted && !c.retry) {
                        c.retry = true;
                        return 0;
                    }
                    c.retry = false;
                    const auto address = lordly_scene_service_address(service);
                    c.calls.append(QJsonObject{{"address", qint64(address)},
                                               {"mode", c.state.mode},
                                               {"alternate", c.state.alternate},
                                               {"counter", c.state.delayed_counter}});
                    if (c.change["address"].toInteger() == address) {
                        c.state.mode = quint8(c.change["mode"].toInt());
                        c.state.alternate = quint8(c.change["alternate"].toInt());
                        c.state.delayed_counter = quint8(c.change["counter"].toInt());
                    }
                    return 1;
                };
                int result, attempts = 0;
                do {
                    result = lordly_scene_schedule(&context.state, invoke, &context,
                                                   interrupted ? 1 : 100);
                    QVERIFY(++attempts < 100);
                    QVERIFY(result != LORDLY_SCENE_INVALID);
                } while (result != LORDLY_SCENE_COMPLETE);
                QCOMPARE(context.calls, row["sequence"].toArray());
                const auto after = row["after"].toObject();
                QCOMPARE(context.state.mode, after["mode"].toInt());
                QCOMPARE(context.state.alternate, after["alternate"].toInt());
                QCOMPARE(context.state.delayed_counter, after["counter"].toInt());
                QCOMPARE(context.state.active, 0);
            }
        }
    }
};
QTEST_GUILESS_MAIN(SceneRuntimeTests)
#include "scene_runtime_tests.moc"
