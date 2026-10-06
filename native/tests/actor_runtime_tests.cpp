#include "engine/actor_instance.h"
#include "engine/actor_motion.h"
#include "engine/actor_pose.h"
#include "engine/scene_actors.h"
#include "engine/scene_camera.h"
#include "engine/scene_fade.h"
#include "engine/scene_projection.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <bit>
#include <limits>
using namespace lordly;
class ActorRuntimeTests : public QObject {
    Q_OBJECT
  private slots:
    void fadeLifecycleAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("scene-fade-mips.cjs"),
             path, work.filePath("fades.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("fades.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(cases.size(), 220);
        for (const auto &v : cases) {
            const auto row = v.toObject();
            SceneFade fade(QByteArray::fromHex(row["initial"].toString().toLatin1()),
                           row["initialActive"].toBool());
            const auto operations = row["operations"].toArray(), states = row["states"].toArray();
            int registrations = 0, frees = 0;
            for (int i = 0; i < operations.size(); ++i) {
                const auto operation = operations[i].toObject();
                const auto kind = operation["kind"].toString();
                bool release = false;
                if (kind == "start") {
                    const auto a = operation["args"].toArray();
                    registrations += fade.start(a[0].toInt(), a[1].toInt(), a[2].toInt(),
                                                a[3].toInt(), a[4].toInt(), a[5].toInt())
                                         ? 1
                                         : 0;
                } else if (kind == "update")
                    release = fade.advance();
                else if (kind == "mark")
                    fade.markRelease();
                else {
                    const bool before = fade.present();
                    fade.cleanup();
                    frees += before && !fade.present() ? 1 : 0;
                }
                const auto expected = states[i].toObject();
                QCOMPARE(fade.record().toHex(), expected["record"].toString().toLatin1());
                QCOMPARE(fade.remaining(), expected["query"].toInt());
                QCOMPARE(fade.slotActive(), expected["active"].toBool());
                QCOMPARE(release, expected["release"].toBool());
                QCOMPARE(registrations, expected["registrations"].toInt());
                QCOMPARE(frees, expected["frees"].toInt());
            }
        }
        SceneFade fade;
        int value = 0;
        QVERIFY(fade.service(0x1b, {3, 50, 100, 150, 255, 0}, value));
        QCOMPARE(fade.rgba(), (std::array<quint8, 4>{50, 100, 150, 255}));
        QVERIFY(!fade.advance());
        QCOMPARE(fade.rgba()[3], 170);
        QVERIFY(fade.service(0x7f, {0, 0}, value));
        QCOMPARE(value, 2);
        QVERIFY(fade.service(0x7e, {}, value));
        QVERIFY(fade.advance());
        QVERIFY(fade.present());
        fade.cleanup();
        QVERIFY(!fade.present());
        auto malformed = QByteArray::fromHex("0002000000000002090a0bff");
        SceneFade invalid(malformed, true);
        QVERIFY_THROWS_EXCEPTION(Error, invalid.advance());
        QCOMPARE(invalid.record(), malformed);
    }
    void actorPresentationAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-style-mips.cjs"),
             path, work.filePath("styles.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("styles.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = QJsonDocument::fromJson(file.readAll()).object();
        const auto rom = Rom::open(path);
        QCOMPARE(data["results"].toArray().size(), 160);
        for (const auto &v : data["results"].toArray()) {
            const auto row = v.toObject();
            SceneActors scene(rom, 2, 0);
            for (const auto &initial : data["initial"].toArray()) {
                const auto a = initial.toObject();
                scene.supplyActor(a["slot"].toInt(),
                                  QByteArray::fromHex(a["record"].toString().toLatin1()));
            }
            int value = 123;
            QVERIFY(scene.service(quint32(row["opcode"].toInt()),
                                  {quint32(row["slot"].toInt()), quint32(row["value"].toInt())},
                                  value));
            QCOMPARE(value, 123);
            for (const auto &after : row["after"].toArray()) {
                const auto a = after.toObject();
                QCOMPARE(scene.actor(a["slot"].toInt())->record().toHex(),
                         a["record"].toString().toLatin1());
            }
        }
    }
    void cameraPlacementAndPicking() {
        QFile file(QFileInfo(QString::fromUtf8(__FILE__))
                       .absoluteDir()
                       .filePath("../../tests/fixtures/cutscene-shared-actor.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(cases.size(), 4);
        for (const auto &v : cases) {
            const auto row = v.toObject();
            const auto values = row["camera"].toArray();
            QByteArray bank(56, 0);
            for (int i = 0; i < 14; ++i)
                put32(bank, i * 4, std::bit_cast<quint32>(float(values[i].toDouble())));
            const SceneCamera camera(bank, 0.1f);
            const auto point = camera.projectPackedOrigin(
                QByteArray::fromHex(row["matrixHex"].toString().toLatin1()));
            const auto expected = row["output"].toArray();
            QVERIFY2(std::abs(point.screen.x() - expected[0].toDouble()) < 0.01 &&
                         std::abs(point.screen.y() - expected[1].toDouble()) < 0.01,
                     qPrintable(row["name"].toString()));
            QVERIFY(point.visible);
        }
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for initializer and command checks");
        const auto rom = Rom::open(path);
        auto cameras = SceneCameras::freshModeTwo(rom);
        QCOMPARE(u32(cameras.actor().bank(), 0), u32(rom.bytes(), 0x1efa68));
        QCOMPARE(u32(cameras.actor().bank(), 20), u32(rom.bytes(), 0x1efa4c));
        const auto original = cameras.actor().bank();
        const QVector<SceneVector> positions{
            {-40, 0, 0}, {240, 0, 55}, {100, 20, -30}, {-85, -17, 127}};
        for (auto position : positions) {
            const auto point = cameras.actor().project(position);
            QVERIFY(point.visible);
            const auto recovered = cameras.actor().unprojectAtHeight(point.screen, position.y);
            QVERIFY2((position - recovered).length() < 0.05f,
                     qPrintable(QString::number((position - recovered).length())));
        }
        QVERIFY(cameras.service(2, 0x35, {0, 0, 0, 0, 0, 400000, 38000}));
        QCOMPARE(cameras.actor().bank(), original);
        QCOMPARE(cameras.registered().bank(), cameras.shared().bank());
        const auto selected = cameras.shared().bank();
        QVERIFY(cameras.service(0, 0x36, {0, 0, 0, 20000, 80000, 600000, 55000}));
        QCOMPARE(cameras.shared().bank(), selected);
        const auto actor = cameras.actor().bank();
        QCOMPARE(actor.mid(4, 16), original.mid(4, 16));
        QCOMPARE(actor.mid(44), original.mid(44));
        QVERIFY_THROWS_EXCEPTION(Error, cameras.service(2, 0x36, {0, 0, 0, 0, 0, 0, 0}));
        QCOMPARE(cameras.actor().bank(), actor);
        QCOMPARE(cameras.shared().bank(), selected);
        QVERIFY(cameras.service(2, 0x36, {0, 0, 0, 0, 0, 400000, 38000}));
        QVERIFY(!cameras.actor().project({0, 0, 5000}).visible);
        QVERIFY_THROWS_EXCEPTION(Error, cameras.actor().unprojectAtHeight({160, 120}, 0));
    }
    void projectionAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("scene-projection-mips.cjs"),
                                 path, work.filePath("projection.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("projection.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(SceneCameras::freshModeTwo(Rom::open(path)).actor().bank().toHex(),
                 fixture["actorCamera"].toString().toLatin1());
        const auto initial = QByteArray::fromHex(fixture["initial"].toString().toLatin1());
        const auto initialized = QByteArray::fromHex(fixture["initialized"].toString().toLatin1());
        LordlySceneProjection raw;
        std::memcpy(raw.bytes, initial.constData(), sizeof(raw.bytes));
        lordly_projection_initialize(&raw);
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(raw.bytes), sizeof(raw.bytes)),
                 initialized);
        QCOMPARE(fixture["cases"].toArray().size(), 13);
        for (const auto &v : fixture["cases"].toArray()) {
            SceneProjection projection(initialized);
            for (const auto &item : v.toObject()["stages"].toArray()) {
                const auto stage = item.toObject();
                const auto target = stage["targets"].toArray();
                auto f = [&](int i) {
                    return std::bit_cast<float>(quint32(target[i].toInteger()));
                };
                projection.start(f(0), f(1), f(2), f(3), stage["duration"].toInt());
                QCOMPARE(projection.record().toHex(), stage["setup"].toString().toLatin1());
                for (const auto &step : stage["steps"].toArray()) {
                    projection.advance();
                    QCOMPARE(projection.record().toHex(), step.toString().toLatin1());
                }
            }
        }
        for (const auto &v : fixture["resets"].toArray()) {
            const auto reset = v.toObject();
            SceneProjection projection(QByteArray::fromHex(reset["before"].toString().toLatin1()));
            QVERIFY(projection.service(2, 0x3d, {quint32(reset["duration"].toInt())}));
            QCOMPARE(projection.record().toHex(), reset["setup"].toString().toLatin1());
            for (const auto &step : reset["steps"].toArray()) {
                projection.advance();
                QCOMPARE(projection.record().toHex(), step.toString().toLatin1());
            }
        }
        QCOMPARE(lordly_projection_advance(nullptr), 0);
        SceneProjection projection;
        QCOMPARE(projection.scaleX(), 1.f);
        QCOMPARE(projection.translateX(), 0.f);
        auto before = projection.record();
        QVERIFY_THROWS_EXCEPTION(Error, projection.start(1, 2, 3, 4, 0));
        QCOMPARE(projection.record(), before);
        QVERIFY_THROWS_EXCEPTION(
            Error, projection.start(std::numeric_limits<float>::infinity(), 2, 3, 4, 1));
        QCOMPARE(projection.record(), before);
        QVERIFY(projection.service(0, 0x2c, {0, 0, 0, 0, 0}));
        QCOMPARE(projection.record(), before);
        QVERIFY(projection.service(2, 0x2c, {quint32(-80000), 30000, 1000, 1000, 1}));
        QCOMPARE(projection.translateX(), 0.f);
        projection.advance();
        QCOMPARE(projection.translateX(), -80.f);
        QCOMPARE(projection.translateY(), 30.f);
        QCOMPARE(projection.remaining(), 0);
        QVERIFY(projection.service(2, 0x2c, {0, 0, 1000, 1000, 65536}));
        before = projection.record();
        projection.advance();
        QCOMPARE(projection.record(), before);
    }
    void bindingAndSlotMotion() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("actor-binding-mips.cjs"),
                                 path, work.filePath("bindings.json")});
        QVERIFY(reference.waitForFinished(60000));
        const auto errors = reference.readAllStandardError();
        QVERIFY2(reference.exitCode() == 0, errors.constData());
        QFile file(work.filePath("bindings.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["results"].toArray();
        QCOMPARE(rows.size(), 8);
        const auto rom = Rom::open(path);
        for (const auto &v : rows) {
            const auto row = v.toObject();
            SceneActors scene(rom, 0, 0);
            ActorBindingInputs inputs;
            for (int i = 0; i < 20; ++i)
                inputs.rows[i] =
                    QByteArray::fromHex(row["rows"].toArray()[i].toString().toLatin1());
            for (int i = 0; i < 5; ++i)
                inputs.unitMembers[i] = quint8(row["members"].toArray()[i].toInt());
            scene.supplyBindingInputs(inputs);
            for (const auto &a : row["initial"].toArray()) {
                const auto r = a.toObject();
                scene.supplyActor(r["slot"].toInt(),
                                  QByteArray::fromHex(r["record"].toString().toLatin1()));
            }
            const int from = row["from"].toInt(), destination = row["destination"].toInt() & 255;
            scene.supplyMotion(from, {2, 0, 0, 3, 0, 0, 1});
            scene.supplyMotion(destination, {5, 0, 0, 4, 0, 0, 1});
            int result = 0;
            QVERIFY(scene.service(0xa6, {quint32(row["destination"].toInt()), 0}, result));
            QJsonArray actual;
            for (int i = 0; i < 28; ++i)
                if (scene.actor(i))
                    actual << QJsonObject{
                        {"slot", i},
                        {"record", QString::fromLatin1(scene.actor(i)->record().toHex())}};
            QCOMPARE(actual, row["slots"].toArray());
            QCOMPARE(scene.motion(from).vx, 2.f);
            QCOMPARE(scene.motion(destination).vx, 5.f);
            scene.advancePlanar();
            if (scene.actor(from))
                QCOMPARE(scene.actor(from)->pose().actor().position.x, 2.f);
            if (scene.actor(destination)) {
                QCOMPARE(scene.actor(destination)->pose().actor().position.x, 5.f);
                const auto saved = scene.actor(destination)->record();
                scene.vacate(destination);
                scene.advancePlanar();
                QCOMPARE(scene.motion(destination).remaining, 3);
                scene.supplyActor(destination, saved);
                scene.advancePlanar();
                QCOMPARE(scene.actor(destination)->pose().actor().position.x, 10.f);
                QCOMPARE(scene.motion(destination).remaining, 2);
            }
        }
        SceneActors scene(rom, 0, 17);
        int value = 123;
        QVERIFY(scene.service(0x14, {4, 30, 1, 2, 0, 0, 0, 0, 0}, value));
        QCOMPARE(u8(scene.actor(4)->record(), 0x13c), 17);
        QVERIFY(scene.service(7, {4, quint32(-1000), quint32(-1000), 4000, 0, 4, 0}, value));
        QVERIFY(scene.service(0x11, {0, 0, 4}, value));
        QCOMPARE(value, 4);
        scene.advancePlanar();
        scene.advancePoses();
        QCOMPARE(scene.actor(4)->pose().actor().position.x, 1.f);
        QVERIFY(scene.service(0x11, {0, 0, 4}, value));
        QCOMPARE(value, 3);
        const auto before = scene.actor(4)->record();
        QVERIFY_THROWS_EXCEPTION(Error, scene.service(3, {0, 30, 1, 2, 0, 0, 0, 0}, value));
        QVERIFY_THROWS_EXCEPTION(Error, scene.service(0xa6, {1, 0}, value));
        QVERIFY_THROWS_EXCEPTION(Error, scene.supplyActor(4, QByteArray(1, 0)));
        QCOMPARE(scene.actor(4)->record(), before);
        QVERIFY(scene.service(7, {0, 0, 0, 0, 0, 0, 0}, value));
        QVERIFY(!scene.motion(0).present);
        QVERIFY(!scene.service(0xdead, {}, value));
    }
    void automaticPortraitAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFINDTESTDATA("actor-portrait-mips.cjs"), path,
                                 work.filePath("portraits.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("portraits.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(cases.size(), 296);
        int checked = 0;
        for (const auto &value : cases) {
            const auto row = value.toObject();
            const int identity = row["identity"].toInt(), bank = row["bank"].toInt();
            if (identity == 255 && (bank < 1 || bank > 68))
                continue; // ActorInstance rejects these absent art banks before lookup.
            SceneActors actors(rom, 2, 0);
            if (identity >= 0) {
                QByteArray record(336, 0);
                put32(record, 0xe8, quint32(bank));
                put8(record, 0x146, quint8(row["variant"].toInt()));
                put8(record, 0x147, quint8(identity));
                actors.supplyActor(7, record);
                if (identity != 255) {
                    QVector<QByteArray> rows(28, QByteArray(248, 0));
                    put32(rows[identity], 0x48, quint32(row["portrait"].toInteger()));
                    actors.supplyBodyRows(rows);
                }
            }
            const auto before = actors.actor(7) ? actors.actor(7)->record() : QByteArray{};
            if (row["error"].toBool())
                QVERIFY_THROWS_EXCEPTION(Error, actors.dialoguePortrait(7));
            else
                QCOMPARE(actors.dialoguePortrait(7), row["value"].toInt());
            QCOMPARE(actors.actor(7) ? actors.actor(7)->record() : QByteArray{}, before);
            ++checked;
        }
        QCOMPARE(checked, 288);
    }
    void scriptActorRelease() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        for (int mode : {0, 1, 2}) {
            SceneActors scene(rom, mode, 0);
            int result = 17;
            for (int slot : {0, 14, 27}) {
                QVERIFY(scene.service(0x14, {quint32(slot), 30, 1, 2, 0, 0, 0, 0, 0}, result));
                scene.supplyMotion(slot, {2, 0, 0, 3, 0, 0, 1});
            }
            const auto survivor = scene.actor(14)->record();
            for (quint32 invalid : {28U, 0x10000U, 0xfffffffeU}) {
                QVERIFY_THROWS_EXCEPTION(Error, scene.service(0x13, {invalid}, result));
                QVERIFY(scene.actor(0) && scene.actor(14) && scene.actor(27));
            }
            QVERIFY_THROWS_EXCEPTION(Error, scene.service(0x13, {}, result));
            for (quint32 slot : {0U, 0U, 27U, 1U}) {
                QVERIFY(scene.service(0x13, {slot}, result));
                QVERIFY(!scene.actor(slot));
                QCOMPARE(scene.actor(14)->record(), survivor);
            }
            QVERIFY(scene.service(0x13, {0xffffffffU}, result));
            for (int slot = 0; slot < 28; ++slot)
                QVERIFY(!scene.actor(slot));
            for (int slot : {0, 14, 27}) {
                QCOMPARE(scene.motion(slot).remaining, 3);
                QCOMPARE(scene.motion(slot).vx, 2.f);
                QCOMPARE(scene.motion(slot).present, 1);
            }
            QCOMPARE(result, 17);
        }
    }
    void recordsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-record-mips.cjs"),
             path, work.filePath("records.json")});
        QVERIFY(reference.waitForFinished(60000));
        const auto errors = reference.readAllStandardError();
        QVERIFY2(reference.exitCode() == 0, errors.constData());
        QFile file(work.filePath("records.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["rows"].toArray();
        QCOMPARE(rows.size(), 82);
        struct Context {
            Rom rom;
            QMap<int, ActorArt> banks;
            QJsonArray terrain;
        } context{Rom::open(path), {}, {}};
        auto resolve = [](void *opaque, int32_t bank, int32_t animation, int32_t facing,
                          int *state) -> int {
            auto &c = *static_cast<Context *>(opaque);
            if (!c.banks.contains(bank))
                c.banks.insert(bank, ActorArt::load(c.rom, bank));
            *state = c.banks[bank].resolve(animation, facing);
            return 1;
        };
        auto height = [](void *opaque, int mode, float x, float z, float *result) -> int {
            static_cast<Context *>(opaque)->terrain << QJsonArray{mode, x, z};
            *result = 7.25f;
            return 1;
        };
        for (const auto &v : rows) {
            const auto row = v.toObject();
            const auto args = row["args"].toArray();
            const bool construct = row["kind"].toString() == "construct";
            LordlyActorSelection selection{
                args[0].toInt(),
                args[1].toInt(),
                args[2].toInt(),
                args[3].toInt(),
                args[construct ? 8 : 7].toInt(),
                {float(args[4].toDouble()), float(args[5].toDouble()), float(args[6].toDouble())},
                uint8_t(construct ? args[7].toInt() : 0),
                uint8_t(row["presentation"].toInt())};
            const auto initial = QByteArray::fromHex(row["initial"].toString().toLatin1());
            LordlyActorRecord record;
            std::memcpy(record.bytes, initial.constData(), sizeof(record.bytes));
            int advance = construct ? 1 : 0;
            context.terrain = {};
            const auto result =
                construct ? lordly_actor_construct(&record, &selection, row["mode"].toInt(),
                                                   resolve, height, &context)
                          : lordly_actor_select(&record, &selection, resolve, &context, &advance);
            QVERIFY2(result == LORDLY_ACTOR_OK, qPrintable(row["name"].toString()));
            const QByteArray actual(reinterpret_cast<const char *>(record.bytes),
                                    sizeof(record.bytes));
            QCOMPARE(actual.toHex(), row["expected"].toString().toLatin1());
            QCOMPARE(advance, row["calls"].toInt());
            QCOMPARE(context.terrain, row["terrain"].toArray());
            LordlyActorPose decoded{};
            lordly_actor_read_pose(&record, &decoded);
            lordly_actor_write_pose(&record, &decoded);
            QCOMPARE(QByteArray(reinterpret_cast<const char *>(record.bytes), sizeof(record.bytes)),
                     actual);
            ActorInstance instance =
                construct ? ActorInstance::create(context.rom, selection, row["mode"].toInt(),
                                                  [](int, float, float) { return 7.25f; })
                          : ActorInstance(context.rom, initial);
            if (!construct)
                instance.select(selection);
            QCOMPARE(instance.record().toHex(), row["fullExpected"].toString().toLatin1());
        }
    }
    void posesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-pose-mips.cjs"),
             path, work.filePath("poses.json")});
        QVERIFY(reference.waitForFinished(60000));
        const auto errors = reference.readAllStandardError();
        QVERIFY2(reference.exitCode() == 0, errors.constData());
        QFile file(work.filePath("poses.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        const auto rows = fixture["rows"].toArray();
        QCOMPARE(rows.size(), 2275);
        QCOMPARE(fixture["unbound"].toInt(), 0);
        const auto rom = Rom::open(path);
        QMap<int, ActorArt> banks;
        int count = 0;
        for (const auto &value : rows) {
            const auto row = value.toObject();
            const int bank = row["bank"].toInt(), state = row["state"].toInt();
            if (!banks.contains(bank))
                banks.insert(bank, ActorArt::load(rom, bank));
            ActorPosePlayer player(rom, banks[bank], state);
            player.actor().position = {1.25f, -2.5f, 3.75f};
            for (const auto &call : row["calls"].toArray()) {
                const auto expected = call.toArray();
                const auto name = QString("bank %1 state %2 invocation %3: %4")
                                      .arg(bank)
                                      .arg(state)
                                      .arg(count)
                                      .arg(player.error());
                QVERIFY2(player.advance() == LORDLY_POSE_OK, qPrintable(name + player.error()));
                const auto &a = player.actor();
                QCOMPARE(a.cursor, expected[0].toInt());
                QCOMPARE(a.delay, expected[1].toInt());
                QCOMPARE(a.frame, expected[2].toInt());
                QCOMPARE(a.state, expected[3].toInt());
                QCOMPARE(a.previous_state, expected[4].toInt());
                QCOMPARE(std::bit_cast<quint32>(a.position.x), quint32(expected[5].toInteger()));
                QCOMPARE(std::bit_cast<quint32>(a.position.y), quint32(expected[6].toInteger()));
                QCOMPARE(std::bit_cast<quint32>(a.position.z), quint32(expected[7].toInteger()));
                QByteArray material(reinterpret_cast<const char *>(a.material), 16);
                material.append(reinterpret_cast<const char *>(a.material_delta), 16);
                QCOMPARE(material.toHex(), expected[8].toString().toLatin1());
                QCOMPARE(player.result(), expected[9].toInt());
                QJsonArray audio;
                for (const auto &r : player.audioRequests())
                    audio << QJsonArray{r.context, r.value};
                QCOMPARE(audio, expected[10].toArray());
                ++count;
            }
        }
        QCOMPARE(count, 54600);
    }
    void poseSuspensionAndBounds() {
        struct Input {
            QVector<LordlyPoseRecord> records;
            int sharedCalls = 0;
            bool available = false;
        };
        Input input{
            {{12, {1, 2, 3}}, {17, {0, 7}}, {13, {255, 200}}, {16, {255, 128}}, {1, {42, 3}}}};
        auto lookup = [](void *context, const LordlyActorPose *actor,
                         LordlyPoseRecord *record) -> int {
            const auto &rows = static_cast<Input *>(context)->records;
            *record = actor->cursor < 0 || actor->cursor >= rows.size() ? LordlyPoseRecord{}
                                                                        : rows[actor->cursor];
            return 1;
        };
        auto shared = [](void *context, const LordlyActorPose *, const LordlyPoseRecord *) -> int {
            auto &i = *static_cast<Input *>(context);
            if (!i.available)
                return 0;
            ++i.sharedCalls;
            return 1;
        };
        LordlyActorPose a{};
        a.cursor = -1;
        LordlyPoseInvocation invocation{};
        QCOMPARE(lordly_actor_advance_pose(&a, &invocation, 1, lookup, shared, &input),
                 LORDLY_POSE_BUDGET);
        QCOMPARE(a.cursor, 0);
        QCOMPARE(a.position.x, 1.f);
        QCOMPARE(a.delay, 0);
        QCOMPARE(invocation.active, 1);
        QCOMPARE(lordly_actor_advance_pose(&a, &invocation, 10, lookup, shared, &input),
                 LORDLY_POSE_SHARED);
        QCOMPARE(a.cursor, 0);
        QCOMPARE(input.sharedCalls, 0);
        input.available = true;
        QCOMPARE(lordly_actor_advance_pose(&a, &invocation, 10, lookup, shared, &input),
                 LORDLY_POSE_OK);
        QCOMPARE(a.position.x, 1.f);
        QCOMPARE(a.position.y, 2.f);
        QCOMPARE(a.position.z, 3.f);
        QCOMPARE(a.cursor, 4);
        QCOMPARE(a.frame, 42);
        QCOMPARE(a.delay, 1);
        QCOMPARE(input.sharedCalls, 1);
        for (const auto value : a.material)
            QCOMPARE(value, 72);
        QCOMPARE(lordly_actor_advance_pose(&a, &invocation, 0, lookup, shared, &input),
                 LORDLY_POSE_OK);
        QCOMPARE(a.delay, -1);
        QCOMPARE(invocation.result, 1);
        for (const auto value : a.material)
            QCOMPARE(value, 0);
        input.records = {{4, {0, 0, 0}}};
        a.cursor = -1;
        a.delay = 0;
        QCOMPARE(lordly_actor_advance_pose(&a, &invocation, 100, lookup, shared, &input),
                 LORDLY_POSE_BUDGET);
        QCOMPARE(a.cursor, -1);
        QCOMPARE(a.delay, 0);
        QCOMPARE(invocation.active, 1);
        input.records = {{13, {16, 77, 0}}};
        QCOMPARE(lordly_actor_advance_pose(&a, &invocation, 10, lookup, shared, &input),
                 LORDLY_POSE_INVALID);
        QCOMPARE(a.cursor, -1);
        QCOMPARE(a.material_delta[0], 128);
        input.records = {{21, {0x34, 0x12, 2}}};
        QCOMPARE(lordly_actor_advance_pose(&a, &invocation, 1, lookup, shared, &input),
                 LORDLY_POSE_OK);
        QCOMPARE(a.frame, 0x1234);
        QCOMPARE(a.delay, 0);
    }
    void movementAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("actor-motion-mips.cjs"),
             path, work.filePath("movement.json")});
        QVERIFY(reference.waitForFinished(60000));
        const auto errors = reference.readAllStandardError();
        QVERIFY2(reference.exitCode() == 0, errors.constData());
        QFile file(work.filePath("movement.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
        const auto constants = actorMotionConstants(Rom::open(path));
        QCOMPARE(constants.tolerance_x, fixture["constants"].toArray()[0].toDouble());
        QCOMPARE(constants.tolerance_z, fixture["constants"].toArray()[1].toDouble());
        QCOMPARE(constants.speed_scale, fixture["constants"].toArray()[2].toDouble());
        const auto rows = fixture["rows"].toArray();
        QCOMPARE(rows.size(), 78);
        for (const auto &v : rows) {
            const auto row = v.toObject(), input = row["input"].toObject(),
                       expected = row["expected"].toObject();
            const auto p = row["inputPositionBits"].toArray(), m = input["motion"].toArray(),
                       args = row["request"].toArray();
            const auto f = [](const QJsonValue &value) { return float(value.toDouble()); };
            const auto fromBits = [](const QJsonValue &value) {
                return std::bit_cast<float>(quint32(value.toInteger()));
            };
            LordlyActorPosition position{fromBits(p[0]), fromBits(p[1]), fromBits(p[2])};
            LordlyActorMotion motion{f(m[0]),
                                     f(m[1]),
                                     f(m[2]),
                                     uint16_t(m[3].toInt()),
                                     uint8_t(m[4].toInt()),
                                     uint8_t(m[5].toInt()),
                                     input["present"].toBool()};
            QJsonArray terrainCalls;
            const auto terrain = [](void *context, int16_t x, int16_t z, float *height) -> int {
                static_cast<QJsonArray *>(context)->append(QJsonArray{x, z});
                *height = 7.25f;
                return 1;
            };
            const auto status = lordly_actor_move(
                input["occupied"].toBool() ? &position : nullptr, &motion, &constants, f(args[0]),
                f(args[1]), f(args[2]), f(args[3]), args[4].toInt(), args[5].toInt(),
                input["mode"].toInt(), uint8_t(input["flags"].toInt()), terrain, &terrainCalls);
            QVERIFY2(status == LORDLY_MOTION_OK, qPrintable(row["name"].toString()));
            const auto ep = expected["positionBits"].toArray(), em = expected["motion"].toArray(),
                       eb = expected["motionBits"].toArray();
            const auto bits = [](float value) { return std::bit_cast<quint32>(value); };
            QCOMPARE(bits(position.x), quint32(ep[0].toInteger()));
            QCOMPARE(bits(position.y), quint32(ep[1].toInteger()));
            QCOMPARE(bits(position.z), quint32(ep[2].toInteger()));
            QCOMPARE(bits(motion.vx), quint32(eb[0].toInteger()));
            QCOMPARE(bits(motion.vy), quint32(eb[1].toInteger()));
            QCOMPARE(bits(motion.vz), quint32(eb[2].toInteger()));
            QCOMPARE(motion.remaining, em[3].toInt());
            QCOMPARE(motion.pause, em[4].toInt());
            QCOMPARE(motion.retained, em[5].toInt());
            QCOMPARE(bool(motion.present), expected["present"].toBool());
            QCOMPARE(terrainCalls, expected["terrain"].toArray());
        }
    }
    void movementLifetimeAndFailures() {
        const LordlyMotionConstants constants{0.1, 0.1, 1000};
        LordlyActorPosition p{0, 10, 0};
        LordlyActorMotion motion{2, 17, 0, 3, 0, 0x5a, 1};
        QCOMPARE(lordly_actor_move(&p, &motion, &constants, -1, -1, 100, 200, 1, 0, 0, 0, nullptr,
                                   nullptr),
                 0);
        for (const auto x : {102.f, 104.f, 106.f}) {
            QCOMPARE(lordly_actor_advance_planar(&p, &motion), 0);
            QCOMPARE(p.x, x);
        }
        QCOMPARE(p.z, 200.f);
        QCOMPARE(p.y, 10.f);
        QCOMPARE(motion.present, 0);
        motion = {2, 17, -3, 0, 1, 0x5a, 1};
        QCOMPARE(lordly_actor_advance_planar(&p, &motion), 0);
        QCOMPARE(motion.present, 0);
        QCOMPARE(p.x, 106.f);
        motion = {2, 17, 0, 0, 0, 0x5a, 1};
        QCOMPARE(lordly_actor_advance_planar(&p, &motion), 0);
        QCOMPARE(motion.remaining, 65535);
        QCOMPARE(motion.present, 1);
        QCOMPARE(lordly_actor_advance_planar(nullptr, &motion), 0);
        QCOMPARE(motion.remaining, 65535);
        for (int i = 0; i < 65535; ++i)
            QCOMPARE(lordly_actor_advance_planar(&p, &motion), 0);
        QCOMPARE(motion.present, 0);
        QCOMPARE(p.x, 131178.f);
        p = {0, 10, 0};
        motion = {2, 17, 0, 3, 0, 0x5a, 1};
        for (const auto duration : {0, 10}) {
            const auto target = duration ? std::numeric_limits<float>::infinity() : 100.f;
            QCOMPARE(lordly_actor_move(&p, &motion, &constants, 20, 30, target, 200, duration, 0, 0,
                                       0, nullptr, nullptr),
                     LORDLY_MOTION_ARITHMETIC);
            QCOMPARE(p.x, 0.f);
            QCOMPARE(motion.vx, 2.f);
            QCOMPARE(motion.remaining, 3);
        }
        QCOMPARE(lordly_actor_move(&p, &motion, &constants, -1, -1, 100, 200, 1, 0, 2, 1, nullptr,
                                   nullptr),
                 LORDLY_MOTION_TERRAIN);
        QCOMPARE(p.x, 0.f);
        QCOMPARE(motion.remaining, 3);
        const LordlyMotionConstants strict{0, 0, 1000};
        motion.pause = 9;
        QCOMPARE(
            lordly_actor_move(&p, &motion, &strict, -1, -1, 0, 0, 0, 2, 0, 0, nullptr, nullptr), 0);
        QCOMPARE(motion.remaining, 1);
        QCOMPARE(motion.pause, 9);
        QCOMPARE(motion.vy, 17.f);
        QCOMPARE(motion.retained, 0x5a);
    }
};
QTEST_GUILESS_MAIN(ActorRuntimeTests)
#include "actor_runtime_tests.moc"
