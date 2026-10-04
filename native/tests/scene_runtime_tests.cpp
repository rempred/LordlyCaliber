#include "engine/actor_matrices.h"
#include "engine/actor_renderer.h"
#include "engine/color_combiner.h"
#include "engine/rom.h"
#include "engine/scene_actors.h"
#include "engine/scene_draw_queue.h"
#include "engine/scene_playback.h"
#include "game/director/actor_render.h"
#include "game/director/scene_matrix.h"
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
    void colorCombinerPrograms() {
        const CombineInputs inputs{{10, 20, 30, 160}, {200, 100, 50, 240},
                                   {128, 64, 255, 128}, {0, 0, 0, 1}, {255, 255, 255, 255}};
        // Independently reduced native formulas. Default: color*primitive,
        // alpha-mask*primitive-alpha+environment-alpha. Tint instead blends
        // toward white. The custom programs sample the other texture in cycle2.
        QCOMPARE(ColorCombiner(0xfc62fe03, 0x2f0d73fd, true).evaluate(inputs),
                 (RenderColor{100, 25, 50, 81}));
        QCOMPARE(ColorCombiner(0xfc62fec3, 0x200d723d, true).evaluate(inputs),
                 (RenderColor{227, 138, 255, 81}));
        QCOMPARE(ColorCombiner(0xfc629603, 0x2f2d7bfd, true).evaluate(inputs),
                 (RenderColor{100, 25, 50, 121}));
        QCOMPARE(ColorCombiner(0xfc2296c3, 0xf22ffa3d, true).evaluate(inputs),
                 (RenderColor{122, 58, 225, 121}));
        QCOMPARE(ColorCombiner(0xfcff97ff, 0xff2fffff, false).evaluate(inputs),
                 (RenderColor{0, 0, 0, 80}));
        auto changed = inputs;
        changed.environment = {40, 80, 120, 1};
        QCOMPARE(ColorCombiner(0xfc2296c3, 0xf22ffa3d, true).evaluate(changed),
                 (RenderColor{154, 90, 248, 121}));
        changed.environment = {255, 255, 255, 255};
        QCOMPARE(ColorCombiner(0xfc2296c3, 0xf22ffa3d, true).evaluate(changed),
                 (RenderColor{255, 158, 255, 255}));
        QVERIFY_THROWS_EXCEPTION(Error, ColorCombiner(0xfb62fe03, 0x2f0d73fd, true));
        QVERIFY_THROWS_EXCEPTION(Error,
            ColorCombiner(0xfc72fe03, 0x2f0d73fd, true).evaluate(inputs));
    }
    void actorMaterialPixelsAndState() {
        QByteArray bank(56, 0), record(336, 0);
        const float cameraValues[]{90, 4.f / 3, 1, 1000, 1, 0, 0, 120, 0, 0, 0, 0, 1, 0};
        for (int i = 0; i < 14; ++i)
            put32(bank, i * 4, std::bit_cast<quint32>(cameraValues[i]));
        for (int i = 0; i < 4; ++i)
            put16(record, 0xa0 + i * 10, 1);
        put8(record, 0, 128);
        put8(record, 0x142, 128);
        put8(record, 0x143, 64);
        put8(record, 0x144, 255);
        ActorFrameLayer frame;
        frame.metadata = {0, {0, 0}, 2, 2, 0, 1024, 1024};
        frame.pixels.image = {2, 2, QByteArray::fromHex("c86432a0c86432a0c86432a0c86432a0")};
        frame.pixels.colorAlpha = QByteArray(4, char(255));
        frame.pixels.dualTexture = true;
        auto state = ActorRenderState::main({0, 0, 0, 1});
        auto render = [&] {
            return ActorRenderer::projectLayer(frame, record, 0, false, SceneCamera(bank, 1.f),
                {255, 255, 255}, 1., 0, 255, &state).image.pixelColor(0, 0);
        };
        QCOMPARE(render(), QColor(100, 25, 50, 81));
        state.apply(QByteArray::fromHex("fc62fec3200d723d"));
        QCOMPARE(render(), QColor(227, 138, 255, 81));
        ActorMaterials materials;
        materials.supplyMaterial(0, QByteArray::fromHex("010000008040ff"));
        state.apply(materials.render(record));
        QCOMPARE(render(), QColor(100, 25, 50, 129));
        materials.supplyMaterial(0, QByteArray::fromHex("002850788040ff"));
        state.apply(materials.render(record));
        QCOMPARE(render(), QColor(79, 55, 118, 129));
        // Malformed trailing commands cannot leave an earlier environment change.
        QVERIFY_THROWS_EXCEPTION(Error, state.apply(QByteArray::fromHex(
            "fb000000ffffffffde00000000000000")));
        QCOMPARE(render(), QColor(79, 55, 118, 129));
        QVERIFY_THROWS_EXCEPTION(Error, state.apply(QByteArray(7, 0)));
        // Original single-texture callback overrides a custom combiner, and
        // a following dual-texture layer does not reset that inherited command.
        frame.pixels.dualTexture = false;
        QCOMPARE(render(), QColor(104, 37, 146, 81));
        frame.pixels.dualTexture = true;
        QCOMPARE(render(), QColor(87, 47, 204, 81));
        frame.pixels.colorAlpha.clear();
        QVERIFY_THROWS_EXCEPTION(Error, render());
    }
    void sceneWorldPassesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFINDTESTDATA("scene-world-passes-mips.cjs"), path,
                                 work.filePath("passes.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("passes.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(rows.size(), 192);
        qsizetype compared = 0;
        for (const auto value : rows) {
            const auto row = value.toObject();
            auto bytes = [](const QJsonValue &v) {
                return QByteArray::fromHex(v.toString().toLatin1());
            };
            std::array<QByteArray, 28> actors;
            QVector<QByteArray> objects;
            for (int i = 0; i < 28; ++i)
                actors[i] = bytes(row["actors"].toArray()[i]);
            for (const auto object : row["objects"].toArray())
                objects.append(bytes(object));
            const auto queue = SceneDrawQueue::fromRecord(bytes(row["queue"]));
            const auto alpha = row["alpha"].toArray();
            const auto passes = queue.modeTwoPasses(actors, objects,
                {qint16(alpha[0].toInt()), qint16(alpha[1].toInt())});
            const auto expected = row["calls"].toArray();
            QCOMPARE(passes.size(), expected.size());
            for (int i = 0; i < passes.size(); ++i) {
                const auto call = expected[i].toArray();
                QCOMPARE(passes[i].kind, call[0].toInt());
                QCOMPARE(passes[i].index, call[1].toInt());
                QCOMPARE(passes[i].shadow, call[2].toInt());
                QCOMPARE(passes[i].alpha, call[3].toInt());
            }
            compared += passes.size();
            objects[0].clear();
            QVERIFY_THROWS_EXCEPTION(Error, queue.modeTwoPasses(actors, objects, {0, 0}));
        }
        qInfo() << "192 original world driver cases; complete callbacks compared:" << compared;
        QByteArray invalid(20, 0);
        put16(invalid, 16, 1);
        put16(invalid, 18, 28);
        QVERIFY_THROWS_EXCEPTION(Error, SceneDrawQueue::fromRecord(invalid).modeTwoPasses({}, {}, {0, 0}));
        put16(invalid, 18, 0);
        QVERIFY_THROWS_EXCEPTION(Error, SceneDrawQueue::fromRecord(invalid).modeTwoPasses({}, {}, {0, 0}));
    }
    void actorWorldMatricesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFINDTESTDATA("actor-world-matrix-mips.cjs"), path,
                                 work.filePath("world.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("world.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(rows.size(), 732);
        const auto rom = Rom::open(path);
        int index = 0;
        for (const auto value : rows) {
            const auto row = value.toObject();
            auto bytes = [&](const QString &key) {
                return QByteArray::fromHex(row[key].toString().toLatin1());
            };
            LordlyActorLighting lighting{};
            for (int i = 0; i < 3; ++i) {
                lighting.point[i] = float(row["point"].toArray()[i].toDouble());
                for (int j = 0; j < 2; ++j)
                    lighting.planar[j][i] =
                        float(row["planar"].toArray()[j].toArray()[i].toDouble());
            }
            const auto matrices = ActorWorldMatrices::prepare(
                rom, bytes("actor"), SceneCamera(bytes("camera"), 0.1f),
                qint16(row["depth"].toInt()), float(row["terrain"].toDouble()), lighting);
            const auto expected = bytes("result");
            for (int i = 0; i < 3; ++i) {
                const auto actual = i < 2 ? matrices.shadows[i] : matrices.matrix;
                QVERIFY2(actual == expected.mid(0x20 + i * 64, 64),
                         qPrintable(QString("World matrix case %1 matrix %2 differs").arg(index).arg(i)));
            }
            ++index;
        }
        qInfo() << "732 original world Actor cases: all 2196 packed matrices match exactly";
    }
    void actorWorldDrawingIsAtomic() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto camera = SceneCameras::freshModeTwo(rom).actor();
        const LordlyActorLighting lighting{{0, 0, 0}, {{20, -40, 50}, {-30, 80, 75}}};
        QByteArray record(336, 0);
        put32(record, 0xe4, 4);
        put32(record, 0xe8, 30);
        for (int i = 0; i < 3; ++i)
            put32(record, 0x104 + i * 4, std::bit_cast<quint32>(1.f));
        put32(record, 0x130, std::bit_cast<quint32>(1.f));
        put32(record, 0x11c, std::bit_cast<quint32>(65537.75f));
        put32(record, 0x124, std::bit_cast<quint32>(-65538.5f));
        put8(record, 0x145, 1);
        int calls = 0;
        SceneActors actors(rom, 2, 0, [&](int mode, float x, float z) {
            if (mode != 2 || x != 1 || z != -2)
                throw Error("Terrain shadow coordinates were not narrowed to signed16");
            ++calls;
            return -12.f;
        });
        actors.supplyActor(0, record);
        auto bad = record;
        put32(bad, 0x130, std::bit_cast<quint32>(std::numeric_limits<float>::quiet_NaN()));
        actors.supplyActor(27, bad);
        const auto before = actors.actorRecords();
        QVERIFY_THROWS_EXCEPTION(Error, actors.prepareModeTwoMatrices(camera, lighting));
        QCOMPARE(actors.actorRecords(), before);
        actors.vacate(27);
        QByteArray depth(4, 0);
        put16(depth, 0, quint16(-20));
        actors.supplyDepthAttachment(4, depth);
        actors.prepareModeTwoMatrices(camera, lighting);
        QVERIFY(calls >= 2);
        const auto after = actors.actor(0)->record();
        QCOMPARE(after.first(0x20), before[0].first(0x20));
        QCOMPARE(after.mid(0xe0), before[0].mid(0xe0));
        const auto expected = ActorWorldMatrices::prepare(rom, before[0], camera, -20, -12, lighting);
        QCOMPARE(after.mid(0xa0, 64), expected.matrix);
        QCOMPARE(after.mid(0x20, 64), expected.shadows[0]);
        QCOMPARE(after.mid(0x60, 64), expected.shadows[1]);
        SceneActors missing(rom, 2, 0);
        missing.supplyActor(0, record);
        QVERIFY_THROWS_EXCEPTION(Error, missing.prepareModeTwoMatrices(camera, lighting));
        QCOMPARE(missing.actor(0)->record(), before[0]);
        actors.setMode(0);
        QVERIFY_THROWS_EXCEPTION(Error, actors.prepareModeTwoMatrices(camera, lighting));
        QCOMPARE(actors.actor(0)->record(), after);
    }
    void sceneDrawQueueAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFINDTESTDATA("scene-draw-queue-mips.cjs"), path,
                                 work.filePath("queue.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("queue.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(rows.size(), 648);
        qsizetype compared = 0;
        for (const auto value : rows) {
            const auto row = value.toObject();
            auto bytes = [](const QJsonValue &v) {
                return QByteArray::fromHex(v.toString().toLatin1());
            };
            std::array<QByteArray, 28> actors;
            QVector<QByteArray> objects;
            const auto sourceActors = row["actors"].toArray();
            for (int i = 0; i < 28; ++i)
                actors[i] = bytes(sourceActors[i]);
            for (const auto object : row["objects"].toArray())
                objects.append(bytes(object));
            const auto queue = SceneDrawQueue::build(
                actors, objects, SceneCamera(bytes(row["camera"]), 1.f),
                {quint32(row["queue"].toDouble()), quint32(row["temporary"].toDouble())});
            QCOMPARE(queue.record(), bytes(row["result"]));
            compared += queue.entries().size();
        }
        qInfo() << "648 complete original queues; entries compared:" << compared;
    }
    void sceneDrawPassesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start("node", {QFINDTESTDATA("scene-draw-passes-mips.cjs"), path,
                                 work.filePath("passes.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("passes.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(rows.size(), 288);
        for (const auto value : rows) {
            const auto row = value.toObject();
            const auto sourceActors = row["actors"].toArray();
            std::array<QByteArray, 28> actors;
            for (int i = 0; i < 28; ++i)
                actors[i] = QByteArray::fromHex(sourceActors[i].toString().toLatin1());
            const auto queue =
                SceneDrawQueue::fromRecord(QByteArray::fromHex(row["queue"].toString().toLatin1()));
            const auto passes = queue.modeZeroPasses(actors, quint8(row["objectChannel"].toInt()),
                                                     quint8(row["channel"].toInt()));
            const auto expected = row["calls"].toArray();
            QCOMPARE(passes.size(), expected.size());
            for (int i = 0; i < passes.size(); ++i) {
                QCOMPARE(passes[i].kind, expected[i].toArray()[0].toInt());
                QCOMPARE(passes[i].index, expected[i].toArray()[1].toInt());
            }
        }
    }
    void sceneDrawQueueBounds() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto cameras = SceneCameras::freshModeTwo(rom);
        ScenePlaybackStart start;
        start.mode = 0;
        const auto directors = Directors::load(rom);
        int selector = 0;
        while (!directors.selectors()[selector])
            ++selector;
        ScenePlayback scene(rom, directors, selector, start, cameras);
        QByteArray actor(336, 0);
        put32(actor, 0xe8, 30);
        put32(actor, 0x11c, std::bit_cast<quint32>(45.f));
        put32(actor, 0x124, std::bit_cast<quint32>(-80.f));
        scene.actors().supplyActor(7, actor);
        const LordlySceneDrawLayout layout{0x80300328, 0x80330000};
        auto queue = scene.drawQueue({}, layout);
        QCOMPARE(queue.record(), SceneDrawQueue::build(scene.actors().actorRecords(), {},
                                                       cameras.registered(), layout)
                                     .record());
        scene.setMode(2);
        QCOMPARE(scene.drawQueue({}, layout).record(),
                 SceneDrawQueue::build(scene.actors().actorRecords(), {}, cameras.actor(), layout)
                     .record());
        QCOMPARE(queue.entries()[0].x, 1350);
        QCOMPARE(queue.entries()[0].z, -2400);
        QCOMPARE(queue.entries()[0].index, 7);
        QVERIFY_THROWS_EXCEPTION(Error, SceneDrawQueue::fromRecord(QByteArray(21, 0)));
        QVERIFY_THROWS_EXCEPTION(Error, SceneDrawQueue::fromRecord(QByteArray(285 * 20, 0)));
        auto missing = queue.record();
        put16(missing, 18, 28);
        QVERIFY_THROWS_EXCEPTION(Error, SceneDrawQueue::fromRecord(missing).modeZeroPasses(
                                            scene.actors().actorRecords(), 0, 0));
        put16(missing, 18, 6);
        QVERIFY_THROWS_EXCEPTION(Error, SceneDrawQueue::fromRecord(missing).modeZeroPasses(
                                            scene.actors().actorRecords(), 0, 0));
        auto actors = scene.actors().actorRecords();
        put32(actors[7], 0x11c, std::bit_cast<quint32>(std::numeric_limits<float>::infinity()));
        QVERIFY_THROWS_EXCEPTION(Error, SceneDrawQueue::build(actors, {}, cameras.actor(), layout));
        QVERIFY_THROWS_EXCEPTION(Error, scene.drawQueue(QVector<QByteArray>(257), layout));
        scene.setMode(0);
        QCOMPARE(queue.record(), scene.drawQueue({}, layout).record());
    }
    void actorPrimitiveColorsAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node", {QFINDTESTDATA("actor-render-mips.cjs"), path, work.filePath("colors.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("colors.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(rows.size(), 216);
        for (const auto value : rows) {
            const auto row = value.toObject();
            const auto actor = QByteArray::fromHex(row["actor"].toString().toLatin1());
            const auto globals = row["global"].toArray();
            int16_t rgb[]{int16_t(globals[0].toInt()), int16_t(globals[1].toInt()),
                          int16_t(globals[2].toInt())};
            uint8_t rgba[4]{};
            QVERIFY(lordly_actor_primitive(
                rgba, reinterpret_cast<const uint8_t *>(actor.constData()), 0,
                uint8_t(row["alpha"].toInt()), uint8_t(row["shadow"].toInt()), rgb));
            const quint32 color =
                quint32(rgba[0]) << 24 | quint32(rgba[1]) << 16 | quint32(rgba[2]) << 8 | rgba[3];
            QCOMPARE(color, quint32(row["color"].toDouble()));
        }
    }
    void actorLayerProjectionAndComposition() {
        QByteArray bank(56, 0);
        const float values[]{90, 4.f / 3, 1, 1000, 1, 0, 0, 120, 0, 0, 0, 0, 1, 0};
        for (int i = 0; i < 14; ++i)
            put32(bank, i * 4, std::bit_cast<quint32>(values[i]));
        SceneCamera camera(bank, 1.f);
        QByteArray actor(336, 0);
        for (int at : {0x20, 0xa0})
            for (int i = 0; i < 4; ++i)
                put16(actor, at + i * 10, 1);
        put8(actor, 0, 128);
        put8(actor, 0x13c, 64);
        put8(actor, 0x142, 128);
        put8(actor, 0x143, 255);
        put8(actor, 0x144, 64);
        ActorFrameLayer frame;
        frame.metadata = {0, {-8, -16}, 16, 16, 0, 1024, 1024};
        frame.pixels.image = {16, 16, QByteArray(16 * 16 * 4, 0)};
        frame.pixels.colorAlpha = QByteArray(16 * 16, char(255));
        frame.pixels.dualTexture = true;
        for (int i = 0; i < 16 * 16; ++i) {
            put8(frame.pixels.image.rgba, i * 4, 200);
            put8(frame.pixels.image.rgba, i * 4 + 1, 100);
            put8(frame.pixels.image.rgba, i * 4 + 2, 50);
            put8(frame.pixels.image.rgba, i * 4 + 3, 255);
        }
        const auto main =
            ActorRenderer::projectLayer(frame, actor, 0, false, camera, {255, 255, 255});
        const auto shadow =
            ActorRenderer::projectLayer(frame, actor, 0, true, camera, {255, 255, 255});
        const QVector<QPointF> expected{{152, 104}, {167, 104}, {167, 119}, {152, 119}};
        QCOMPARE(main.plane.clip.size(), 4);
        for (int i = 0; i < 4; ++i)
            QVERIFY(QLineF(main.plane.clip[i], expected[i]).length() < 0.0001);
        QCOMPARE(main.image.pixelColor(0, 0), QColor(100, 100, 12, 128));
        QCOMPARE(shadow.image.pixelColor(0, 0), QColor(0, 0, 0, 32));
        actor.replace(0x60, 64, actor.mid(0x20, 64));
        put16(actor, 0x60 + 24, 20);
        const auto secondShadow = ActorRenderer::projectLayer(
            frame, actor, 0, true, camera, {255, 255, 255}, 1., 1, 128);
        QCOMPARE(secondShadow.shadowIndex, 1);
        QCOMPARE(secondShadow.image.pixelColor(0, 0), QColor(0, 0, 0, 16));
        QCOMPARE(secondShadow.plane.clip.size(), 4);
        for (int i = 0; i < 4; ++i)
            QVERIFY(QLineF(secondShadow.plane.clip[i], expected[i] + QPointF(20, 0)).length() < 0.0001);
        QVERIFY_THROWS_EXCEPTION(Error, ActorRenderer::projectLayer(
            frame, actor, 0, true, camera, {255, 255, 255}, 1., 2));
        QImage composed(320, 240, QImage::Format_RGBA8888);
        composed.fill(Qt::transparent);
        ActorRenderer::paint(composed, {main});
        QCOMPARE(composed.pixelColor(150, 110).alpha(), 0);
        QVERIFY(composed.pixelColor(155, 110).alpha() > 0);
        auto foreground = main;
        foreground.image.fill(QColor(10, 20, 240, 255));
        ActorRenderer::paint(composed, {foreground});
        QCOMPARE(composed.pixelColor(155, 110), QColor(10, 20, 240, 255));
        auto invalid = frame;
        invalid.metadata.width = 17;
        QVERIFY_THROWS_EXCEPTION(
            Error, ActorRenderer::projectLayer(invalid, actor, 0, false, camera, {255, 255, 255}));
        QVERIFY_THROWS_EXCEPTION(
            Error, ActorRenderer::projectLayer(frame, actor, 16, false, camera, {255, 255, 255}));
        QByteArray classes(144, 0);
        put8(classes, 4, 2);
        put8(classes, 72 + 4, 1);
        put8(classes, 69, 1);
        uint8_t size = 0;
        QVERIFY(lordly_actor_render_class_size(
            reinterpret_cast<const uint8_t *>(classes.constData()), 2, 0, 1, &size));
        QCOMPARE(size, 2);
        put8(classes, 69, 0);
        QVERIFY(lordly_actor_render_class_size(
            reinterpret_cast<const uint8_t *>(classes.constData()), 2, 0, 1, &size));
        QCOMPARE(size, 1);
        QVERIFY(!lordly_actor_render_class_size(
            reinterpret_cast<const uint8_t *>(classes.constData()), 2, 2, 0, &size));
    }
    void actorRendererUsesBodyVariant() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto camera = SceneCameras::freshModeTwo(rom).actor();
        ActorRenderer renderer(rom);
        bool found = false;
        for (int source = 1; source < 165 && !found; ++source) {
            ActorArt art = ActorArt::loadBody(rom, source, source, 0, 0);
            if (!art.bodyVariant())
                continue;
            const auto frame = art.frame(rom, 0, art.bodyVariant()), wrong = art.frame(rom, 0, 0);
            if (frame.isEmpty() || wrong.isEmpty() ||
                frame[0].pixels.image.rgba == wrong[0].pixels.image.rgba)
                continue;
            QByteArray record(336, 0);
            for (int i = 0; i < 16; ++i)
                put8(record, i, 255);
            for (int i = 0; i < 3; ++i)
                put8(record, 0x142 + i, 255);
            for (int i = 0; i < 4; ++i)
                put16(record, 0xa0 + i * 10, 1);
            put32(record, 0xe8, source);
            put32(record, 0xec, source);
            put8(record, 0x13d, 1);
            ActorInstance actor(rom, record);
            const auto rendered = renderer.project(actor, false, camera, {255, 255, 255});
            QCOMPARE(rendered.size(), frame.size());
            const auto &r = frame[0].pixels.image;
            const QImage expected(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width,
                                  r.height, r.width * 4, QImage::Format_RGBA8888);
            QCOMPARE(rendered[0].image, expected);
            const auto cached = renderer.project(actor, false, camera, {255, 255, 255});
            QCOMPARE(cached[0].image, expected);
            qInfo() << "Body image variant" << art.bodyVariant() << "from class" << source
                    << "retained across cached redraw";
            found = true;
        }
        QVERIFY(found);
    }
    void actorMainMatricesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node", {QFINDTESTDATA("actor-matrix-mips.cjs"), path, work.filePath("actors.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("actors.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
        QCOMPARE(rows.size(), 468);
        const auto rom = Rom::open(path);
        qint64 maximum = 0;
        for (const auto value : rows) {
            const auto row = value.toObject();
            auto bytes = [&](const QString &key) {
                return QByteArray::fromHex(row[key].toString().toLatin1());
            };
            auto changedRom = rom.bytes();
            const auto aspect = std::bit_cast<quint64>(row["probeAspect"].toDouble());
            put32(changedRom, 0x2a8b70, quint32(aspect >> 32));
            put32(changedRom, 0x2a8b74, quint32(aspect));
            const auto result =
                ActorMatrices::prepare(Rom::fromBytes(changedRom), bytes("actor"), bytes("channel"),
                                       SceneCamera(bytes("camera"), 0.1f));
            const auto expected = bytes("result").mid(0xa0, 64),
                       expectedShadow = bytes("result").mid(0x20, 64);
            const auto prepass = row["prepass"].toObject();
            const auto position = QByteArray::fromHex(prepass["position"].toString().toLatin1()),
                       scale = QByteArray::fromHex(prepass["scale"].toString().toLatin1());
            QVERIFY(qAbs(result.position.x() - std::bit_cast<float>(u32(position, 0))) < 0.0001);
            QVERIFY(qAbs(result.position.y() - std::bit_cast<float>(u32(position, 4))) < 0.0001);
            QVERIFY(qAbs(result.scale.x() - std::bit_cast<float>(u32(scale, 0))) < 0.000001);
            QVERIFY(qAbs(result.scale.y() - std::bit_cast<float>(u32(scale, 4))) < 0.000001);
            auto fixed = [](const QByteArray &b, int i) {
                return qint32(quint32(u16(b, i * 2)) << 16 | u16(b, 32 + i * 2));
            };
            for (int i = 0; i < 32; ++i) {
                const qint64 difference =
                    qAbs(qint64(fixed(i < 16 ? result.matrix : result.shadow, i % 16)) -
                         fixed(i < 16 ? expected : expectedShadow, i % 16));
                maximum = qMax(maximum, difference);
                QVERIFY2(difference <= 4,
                         qPrintable(QString("Actor matrix field %1 differs by %2 units")
                                        .arg(i)
                                        .arg(difference)));
            }
        }
        qInfo() << "468 original Actor main/shadow matrices; maximum fixed-point difference:"
                << maximum;
    }
    void actorMainDrawingIsAtomic() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto resources = Directors::load(rom);
        int selector = 0;
        while (!resources.selectors()[selector])
            ++selector;
        ScenePlaybackStart start;
        start.mode = 0;
        auto cameras = SceneCameras::freshModeTwo(rom);
        ScenePlayback scene(rom, resources, selector, start, cameras);
        QByteArray record(336, 0);
        put32(record, 0xe8, 30);
        for (int i = 0; i < 3; ++i)
            put32(record, 0x104 + i * 4, std::bit_cast<quint32>(1.f));
        scene.actors().supplyActor(0, record);
        const auto before = scene.actors().actor(0)->record(),
                   channel = scene.transforms().channel(0);
        auto bad = record;
        put8(bad, 0x13e, 20);
        scene.actors().supplyActor(27, bad);
        QVERIFY_THROWS_EXCEPTION(Error, scene.prepareModeZeroDrawing(20));
        QCOMPARE(scene.actors().actor(0)->record(), before);
        QCOMPARE(scene.transforms().channel(0), channel);
        QCOMPARE(scene.completedUpdates(), 0U);
        put8(bad, 0x13e, 0);
        put32(bad, 0x130, std::bit_cast<quint32>(std::numeric_limits<float>::quiet_NaN()));
        scene.actors().supplyActor(27, bad);
        QVERIFY_THROWS_EXCEPTION(Error, scene.prepareModeZeroDrawing(20));
        QCOMPARE(scene.actors().actor(0)->record(), before);
        QCOMPARE(scene.transforms().channel(0), channel);
        bad = before;
        put8(bad, 0x13f, 10);
        QVERIFY_THROWS_EXCEPTION(Error,
                                 ActorMatrices::prepare(rom, bad, channel, cameras.registered()));
        scene.actors().supplyActor(27, record);
        scene.prepareModeZeroDrawing(20);
        const auto drawing = ActorMatrices::prepare(rom, before, scene.transforms().channel(0),
                                                    cameras.registered());
        auto expected = before;
        expected.replace(0xa0, 64, drawing.matrix);
        expected.replace(0x20, 64, drawing.shadow);
        expected.replace(0x60, 64, drawing.shadow);
        put32(expected, 0xfc, std::bit_cast<quint32>(float(drawing.position.x())));
        put32(expected, 0x100, std::bit_cast<quint32>(float(drawing.position.y())));
        QCOMPARE(scene.actors().actor(0)->record(), expected);
        QCOMPARE(scene.actors().actor(27)->record(), expected);
        QCOMPARE(scene.completedUpdates(), 0U);
        const auto prepared = scene.transforms().channel(0);
        QVERIFY_THROWS_EXCEPTION(Error, scene.prepareModeZeroDrawing(21));
        QCOMPARE(scene.transforms().channel(0), prepared);
        QCOMPARE(scene.actors().actor(0)->record(), expected);
        scene.setMode(2);
        QVERIFY_THROWS_EXCEPTION(Error, scene.prepareModeZeroDrawing(20));
        QCOMPARE(scene.transforms().channel(0), prepared);
        QCOMPARE(scene.actors().actor(0)->record(), expected);
    }
    void sceneMatricesAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node", {QFINDTESTDATA("scene-matrix-mips.cjs"), path, work.filePath("matrices.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile input(work.filePath("matrices.json"));
        QVERIFY(input.open(QIODevice::ReadOnly));
        const auto cases = QJsonDocument::fromJson(input.readAll()).object()["cases"].toArray();
        QCOMPARE(cases.size(), 36);
        const auto rom = Rom::open(path);
        qint64 maxDifference = 0;
        for (const auto entry : cases) {
            const auto row = entry.toObject();
            const auto initial = QByteArray::fromHex(row["initial"].toString().toLatin1());
            const auto expected = QByteArray::fromHex(row["result"].toString().toLatin1());
            SceneTransforms state;
            for (int i = 0; i < 20; ++i)
                state.supplyChannel(i, initial.mid(i * 88, 88));
            const auto camera =
                SceneCamera(QByteArray::fromHex(row["camera"].toString().toLatin1()), 1.f);
            state.prepareMatrices(rom, camera, row["count"].toInt());
            for (int i = 0; i < 20; ++i) {
                const auto actual = state.channel(i), target = expected.mid(i * 88, 88);
                QCOMPARE(actual.mid(64), target.mid(64));
                if (i >= row["count"].toInt()) {
                    QCOMPARE(actual, target);
                    continue;
                }
                QCOMPARE(state.layerMatrix(i),
                         QByteArray::fromHex(row["layers"].toArray()[i].toString().toLatin1()));
                auto fixed = [](const QByteArray &b, int field) {
                    return qint32(quint32(u16(b, field * 2)) << 16 | u16(b, 32 + field * 2));
                };
                for (int field = 0; field < 16; ++field) {
                    const auto difference =
                        qAbs(qint64(fixed(actual, field)) - fixed(target, field));
                    maxDifference = qMax(maxDifference, difference);
                    // Allow two 16.16 least-significant units for host trig rounding.
                    // Final pixel accuracy is a separate rendering concern.
                    QVERIFY2(difference <= 2,
                             qPrintable(QString("Channel %1 field %2 differs by %3 fixed units")
                                            .arg(i)
                                            .arg(field)
                                            .arg(difference)));
                }
            }
        }
        qInfo() << "36 camera/count cases, 300 matrices; maximum 16.16 difference:"
                << maxDifference;
    }
    void sceneMatrixProjectionAndBounds() {
        float matrix[16], scale[16];
        lordly_matrix_translate(matrix, 10.f, 20.f, -30.f);
        lordly_matrix_scale(scale, 2.f, 3.f, 4.f);
        lordly_matrix_multiply(matrix, matrix, scale);
        QCOMPARE(matrix[12], 20.f);
        QCOMPARE(matrix[13], 60.f);
        QCOMPARE(matrix[14], -120.f);
        QByteArray packed(64, 0);
        QVERIFY(lordly_matrix_pack(reinterpret_cast<uint8_t *>(packed.data()), matrix));
        const auto before = packed;
        matrix[12] = 32768.f;
        QVERIFY(!lordly_matrix_pack(reinterpret_cast<uint8_t *>(packed.data()), matrix));
        QCOMPARE(packed, before);
        QByteArray bank(56, 0);
        const float values[14] = {90, 4.f / 3, 0.1f, 1000, 1, 0, 0, 120, 0, 0, 0, 0, 1, 0};
        for (int i = 0; i < 14; ++i)
            put32(bank, i * 4, std::bit_cast<quint32>(values[i]));
        const SceneCamera camera(bank, 1);
        // Translated/scaled (5, -10, 30) is world (30,30,0), hence (190,90).
        const auto point = camera.projectPackedPoint(packed, {5, -10, 30});
        QVERIFY(qAbs(point.screen.x() - 190) < 0.0001);
        QVERIFY(qAbs(point.screen.y() - 90) < 0.0001);
        QVERIFY(point.visible);
        // Eye at Z10, near Z8. A tilted 2x2 plane spans Z7..9, so exactly
        // its left half survives. Expected screen corners are hand-computed.
        put32(bank, 8, std::bit_cast<quint32>(2.f));
        put32(bank, 28, std::bit_cast<quint32>(10.f));
        const SceneCamera clipping(bank, 1);
        lordly_matrix_identity(matrix);
        QByteArray identityMatrix(64, 0);
        QVERIFY(lordly_matrix_pack(reinterpret_cast<uint8_t *>(identityMatrix.data()), matrix));
        const auto plane =
            clipping.projectPackedPlane(identityMatrix, {-1, 1, 7}, {1, 0, 1}, {0, -1, 0}, {2, 2});
        QCOMPARE(plane.clip.size(), 4);
        const QVector<QPointF> expected{{120, 80}, {160, 60}, {160, 180}, {120, 160}};
        for (const auto p : expected) {
            bool found = false;
            for (const auto actual : plane.clip)
                found |= qAbs(actual.x() - p.x()) < 0.001 && qAbs(actual.y() - p.y()) < 0.001;
            QVERIFY(found);
        }
        QVERIFY(
            clipping.projectPackedPlane(identityMatrix, {-1, 1, 11}, {1, 0, 1}, {0, -1, 0}, {2, 2})
                .clip.isEmpty());
        QVERIFY(
            clipping
                .projectPackedPlane(identityMatrix, {-1, 1, -2000}, {1, 0, 0}, {0, -1, 0}, {2, 2})
                .clip.isEmpty());
        QVERIFY_THROWS_EXCEPTION(
            Error, clipping.projectPackedPlane(identityMatrix, {}, {1, 0, 0}, {0, 1, 0}, {0, 1}));
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required for current-ROM matrix constants");
        const auto rom = Rom::open(path);
        SceneTransforms state;
        const auto identity = state.channel(0);
        auto overflow = state.channel(19);
        put32(overflow, 0x40, std::bit_cast<quint32>(1e30f));
        state.supplyChannel(19, overflow);
        QVERIFY_THROWS_EXCEPTION(Error, state.prepareMatrices(rom, camera, 20));
        QCOMPARE(state.channel(0), identity);
        QCOMPARE(state.channel(19), overflow);
        QVERIFY_THROWS_EXCEPTION(Error, state.prepareMatrices(rom, camera, 21));
        QCOMPARE(state.channel(0), identity);
    }
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
        QByteArray actor(336, char(-89));
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
