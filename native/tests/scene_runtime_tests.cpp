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
        const QVector<quint32> setup{0x14, 0,    30, 1, 2, 1000, 2000,  3000,  0,    0,
                                     0x7b, 0,    1,  5, 3, 0x2c, 10000, 20000, 1000, 1000,
                                     2,    0x1b, 3,  5, 6, 7,    255,   0};
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
        // This fixture explicitly has no terrain motion, sprites, channels or
        // other scheduled objects. Their callbacks acknowledge those empty pools.
        auto emptyDomains = [](LordlySceneService) { return true; };
        auto baseline = create(0);
        QCOMPARE(baseline.advance(), ScenePlayback::Suspended);
        QCOMPARE(baseline.pendingService(), std::optional{LORDLY_SCENE_COLOR});
        QCOMPARE(baseline.completedUpdates(), 0U);
        QVector<QByteArray> actors, projections, fades;
        QVector<int> cursors;
        const float heights[] = {2, 7, 12, 12};
        const int counts[] = {2, 1, 0, 0};
        for (int tick = 0; tick < 4; ++tick) {
            QCOMPARE(baseline.advance(emptyDomains),
                     tick == 3 ? ScenePlayback::Released : ScenePlayback::Advanced);
            QCOMPARE(baseline.completedUpdates(), quint64(tick + 1));
            QCOMPARE(baseline.director().state().counter, quint32(tick + 1));
            QCOMPARE(baseline.actors().actor(0)->pose().actor().position.y, heights[tick]);
            QCOMPARE(baseline.projection().remaining(), counts[tick]);
            QCOMPARE(baseline.fade().rgba()[3], 255); // Separate scheduler slot.
            actors << baseline.actors().actor(0)->record();
            projections << baseline.projection().record();
            fades << baseline.fade().record();
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
            QCOMPARE(resumed.director().state().resume_cursor, cursors[tick]);
            QCOMPARE(resumed.director().state().counter, quint32(tick + 1));
            QCOMPARE(resumed.completedUpdates(), quint64(tick + 1));
        }
        auto switched = create(1);
        QCOMPARE(switched.advance(emptyDomains), ScenePlayback::Advanced);
        QCOMPARE(switched.schedule().alternate, 1);
        QCOMPARE(switched.advance(), ScenePlayback::Advanced); // Alternate needs no other domains.
        QCOMPARE(switched.schedule().alternate, 0);
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
