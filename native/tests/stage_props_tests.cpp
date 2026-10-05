#include "engine/direct_pose.h"
#include "engine/scene_world_drawing.h"
#include "engine/stage_orthographic.h"
#include "engine/stage_placements.h"
#include "game/director/scene_matrix.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class StagePropsTests : public QObject {
    Q_OBJECT
    QJsonObject reference_;
    struct CustomContext {
        QJsonArray programs;
        int random = 0, sounds = 0;
    };
    static uint32_t random(void *ptr) {
        static constexpr uint32_t values[]{13, 32767, 0, 71, 111, 99};
        return values[static_cast<CustomContext *>(ptr)->random++ % 6];
    }
    static int lookup(void *ptr, const uint8_t *bytes, LordlyPoseRecord *out) {
        const QByteArray record(reinterpret_cast<const char *>(bytes), 80);
        const int state = qint16(u16(record, 4)), cursor = qint16(u16(record, 6));
        const auto &programs = static_cast<CustomContext *>(ptr)->programs;
        if (state < 0 || state >= programs.size())
            return 0;
        const auto program = programs[state].toArray();
        if (cursor < 0 || cursor >= program.size()) {
            out->opcode = 0;
            return 1;
        }
        const auto command = program[cursor].toArray();
        out->opcode = quint8(command[0].toInt());
        for (int i = 1; i < command.size(); ++i)
            out->operands[i - 1] = quint8(command[i].toInt());
        return 1;
    }
    static int sound(void *ptr, const uint8_t *, const LordlyPoseRecord *) {
        static_cast<CustomContext *>(ptr)->sounds++;
        return 1;
    }
    static QByteArray unhex(const QJsonValue &value) {
        return QByteArray::fromHex(value.toString().toLatin1());
    }
  private slots:
    void allOrthographicProps() {
        const auto rom = Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));
        float identity[16];
        lordly_matrix_identity(identity);
        QByteArray parent(64, 0);
        QVERIFY(lordly_matrix_pack(reinterpret_cast<uint8_t *>(parent.data()), identity));
        const auto camera = SceneCamera::orthographic(SceneWorldDrawing::clipMatrix());
        StageFlatRenderer renderer(rom);
        int count = 0, visible = 0;
        for (int selector = 0; selector < 80; ++selector) {
            auto stage = StageOrthographic::load(rom, StagePlacements::load(rom, selector),
                                                 [] { return 1234U; });
            count += int(stage.props().size());
            stage.advance();
            try {
                for (int pass = 0; pass < 3; ++pass) {
                    const auto layers =
                        renderer.project(stage, pass, camera, parent, selector, {255, 255, 255});
                    for (const auto &layer : layers) {
                        const auto &prop = stage.props()[layer.slot];
                        const auto pixels = prop.pose.art()
                                                .frame(rom, qint16(u16(prop.pose.record(), 10)),
                                                       u16(prop.header, 6))[layer.layer]
                                                .pixels;
                        // Stage mux is color*primitive, mask*primitive-alpha.
                        // Check texture values independently of screen clipping.
                        for (int y = 0; y < layer.image.height(); ++y)
                            for (int x = 0; x < layer.image.width(); ++x) {
                                const int at = (y * layer.image.width() + x) * 4;
                                const auto color = layer.image.pixelColor(x, y);
                                const int expected =
                                    u8(pixels.image.rgba, at + 3) * layer.primitive[3] / 255;
                                QVERIFY(qAbs(color.alpha() - expected) <= 1);
                            }
                    }
                    QImage image(320, 240, QImage::Format_RGBA8888);
                    image.fill(Qt::transparent);
                    ActorRenderer::paint(image, layers);
                    if (selector == 57) {
                        for (int y = 0; y < 240; ++y)
                            for (int x = 0; x < 320; ++x)
                                visible += image.pixelColor(x, y).alpha() != 0;
                        QVERIFY(image.save(QString("stage-hall-pass-%1.png").arg(pass)));
                    }
                }
            } catch (const Error &e) {
                QFAIL(qPrintable(QString("Selector%1: %2").arg(selector).arg(e.what())));
            }
        }
        QCOMPARE(count, 116);
        QVERIFY(visible > 100);
    }
    void passSelectionAndWrapping() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto rom = Rom::open(path);
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("stage-flat-mips.cjs"),
             path, work.filePath("passes.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile file(work.filePath("passes.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        for (const auto &value :
             QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray()) {
            const auto row = value.toObject();
            const auto z = row["z"].toArray();
            for (int i = 0; i < z.size(); ++i) {
                QByteArray record(100, 0);
                put16(record, 4, quint16(z[i].toInt()));
                int expected = -1;
                for (int pass = 0; pass < 3; ++pass)
                    if (row["passes"].toArray()[pass].toArray().contains(i))
                        expected = pass;
                QCOMPARE(StageOrthographic::drawPass(record, float(row["base"].toDouble()),
                                                     float(row["foreground"].toDouble())),
                         expected);
            }
        }
        for (const auto &value : reference_["wrapping"].toArray()) {
            const auto row = value.toObject();
            auto stage = StageOrthographic::fromRecords(rom, {unhex(row["initial"])});
            for (const auto &call : row["calls"].toArray()) {
                stage.advance();
                QCOMPARE(stage.props()[0].record(), unhex(call));
            }
        }
    }
    void initTestCase() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess reference;
        reference.start(
            "node",
            {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("stage-props-mips.cjs"),
             path, work.filePath("reference.json")});
        QVERIFY(reference.waitForFinished(60000));
        QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
        QFile output(work.filePath("reference.json"));
        QVERIFY(output.open(QIODevice::ReadOnly));
        reference_ = QJsonDocument::fromJson(output.readAll()).object();
        qInfo().noquote() << reference.readAllStandardOutput();
    }
    void placementReadersAgainstMips() {
        const auto rom = Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));
        int ordinary = 0, special = 0;
        for (const auto &value : reference_["placements"].toArray()) {
            const auto row = value.toObject();
            const bool perspective = row["kind"].toInt();
            StagePlacement placement{perspective ? StagePlacementKind::Perspective
                                                 : StagePlacementKind::Orthographic,
                                     unhex(row["source"])};
            QCOMPARE(placement.resolve(QByteArray(perspective ? 36 : 24, '\xa7')),
                     unhex(row["resolved"]));
            const int selector = row["selector"].toInt(), index = row["index"].toInt();
            if (selector < 0)
                continue;
            const auto data = StagePlacements::load(rom, selector);
            const auto &rows = perspective ? data.perspective() : data.orthographic();
            if (qint16(u16(placement.source, 0)) == -1)
                QCOMPARE(rows.size(), index);
            else {
                QVERIFY(index < rows.size());
                QCOMPARE(rows[index].source, placement.source);
                placement.randomized() ? ++special : ++ordinary;
            }
        }
        QCOMPARE(ordinary, 165);
        QCOMPARE(special, 3);
        QVERIFY_THROWS_EXCEPTION(Error, StagePlacements::load(rom, 80));
        auto bytes = rom.bytes();
        put16(bytes, 0x8a99ba + 18, 0xffff);
        QVERIFY_THROWS_EXCEPTION(Error, StagePlacements::load(Rom::fromBytes(bytes), 0));
        put16(bytes, 0x8a99ba + 18, 4824);
        put16(bytes, 0x8a99ba + 4824, 0x7fff);
        QVERIFY_THROWS_EXCEPTION(Error, StagePlacements::load(Rom::fromBytes(bytes), 0));
    }
    void poseInitializationAgainstMips() {
        for (const auto &value : reference_["initializations"].toArray()) {
            const auto row = value.toObject();
            auto actual = unhex(row["initial"]);
            CustomContext context;
            QCOMPARE(lordly_direct_initialize(reinterpret_cast<uint8_t *>(actual.data()),
                                              quint32(row["selector"].toDouble()), random,
                                              &context),
                     LORDLY_POSE_OK);
            QCOMPARE(actual, unhex(row["record"]));
            QCOMPARE(context.random, row["random"].toInt());
        }
    }
    void retailPosesAgainstMips() {
        const auto rom = Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));
        for (const auto &value : reference_["rows"].toArray()) {
            const auto row = value.toObject();
            DirectPosePlayer player(rom, unhex(row["initial"]));
            for (const auto &snapshot : row["calls"].toArray()) {
                const auto call = snapshot.toObject();
                QVERIFY2(player.advance(10000) == LORDLY_POSE_OK, qPrintable(player.error()));
                QCOMPARE(player.record(), unhex(call["record"]));
                QCOMPARE(player.sounds().size(), call["soundCount"].toInt());
            }
        }
    }
    void poseControlAndBounds() {
        for (const auto &value : reference_["custom"].toArray()) {
            const auto row = value.toObject();
            auto actual = unhex(row["initial"]);
            CustomContext context{row["programs"].toArray()};
            // The random-mode initializer consumed three random values already.
            if (u8(actual, 0x48) & 2)
                context.random = 3;
            LordlyPoseInvocation invocation{};
            for (const auto &snapshot : row["calls"].toArray()) {
                const auto call = snapshot.toObject();
                context.sounds = 0;
                int result;
                do {
                    result = lordly_direct_advance(reinterpret_cast<uint8_t *>(actual.data()),
                                                   &invocation, 1, lookup, sound, random, &context);
                } while (result == LORDLY_POSE_BUDGET);
                QCOMPARE(result, LORDLY_POSE_OK);
                QCOMPARE(actual, unhex(call["record"]));
                QCOMPARE(context.sounds, call["soundCount"].toInt());
                QCOMPARE(context.random, call["random"].toInt());
            }
        }
        QByteArray actual(80, 0);
        LordlyPoseInvocation invocation{};
        QCOMPARE(lordly_direct_initialize(reinterpret_cast<uint8_t *>(actual.data()), 0, nullptr,
                                          nullptr),
                 LORDLY_POSE_OK);
        CustomContext context{QJsonDocument::fromJson("[[[15,0]]]").array()};
        QCOMPARE(lordly_direct_advance(reinterpret_cast<uint8_t *>(actual.data()), &invocation, 100,
                                       lookup, sound, nullptr, &context),
                 LORDLY_POSE_INVALID);
        QCOMPARE(u8(actual, 0x46), 4); // Reject the fifth push without corrupting adjacent fields.
        actual.fill(0);
        put8(actual, 0x48, 2);
        const auto prior = actual;
        QCOMPARE(lordly_direct_initialize(reinterpret_cast<uint8_t *>(actual.data()), 0, nullptr,
                                          nullptr),
                 LORDLY_POSE_SHARED);
        QCOMPARE(actual, prior);
    }
};
QTEST_GUILESS_MAIN(StagePropsTests)
#include "stage_props_tests.moc"
