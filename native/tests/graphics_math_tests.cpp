#include "engine/scene_camera.h"
#include "game/director/graphics_math.h"
#include "game/director/scene_matrix.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
using namespace lordly;
class GraphicsMathTests : public QObject {
    Q_OBJECT
    QJsonObject reference_;
    static QByteArray bank(const QJsonArray &values) {
        QByteArray bytes(56, 0);
        for (int i = 0; i < 14; ++i)
            put32(bytes, i * 4, std::bit_cast<quint32>(float(values[i].toDouble())));
        return bytes;
    }
    static QByteArray encoded(const float matrix[16]) {
        QByteArray bytes(64, 0);
        for (int i = 0; i < 16; ++i)
            put32(bytes, i * 4, std::bit_cast<quint32>(matrix[i]));
        return bytes;
    }
  private slots:
    void initTestCase() {
        const auto rom = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (rom.isEmpty())
            return;
        QTemporaryDir temporary;
        QProcess process;
        process.start("node", {QFINDTESTDATA("graphics-math-mips.cjs"), rom,
                              temporary.filePath("reference.json")});
        QVERIFY(process.waitForFinished(60000));
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        QFile file(temporary.filePath("reference.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        reference_ = QJsonDocument::fromJson(file.readAll()).object();
    }
    void mathAgainstOriginalInstructions() {
        if (reference_.isEmpty())
            QSKIP("Private ROM required for original-instruction comparison");
        const auto trig = reference_["trig"].toArray();
        QCOMPARE(trig.size(), 2066);
        for (const auto &entry : trig) {
            const auto row = entry.toObject();
            const auto input = std::bit_cast<float>(quint32(row["input"].toInteger()));
            QCOMPARE(std::bit_cast<quint32>(lordly_graphics_sin(input)), quint32(row["sin"].toInteger()));
            QCOMPARE(std::bit_cast<quint32>(lordly_graphics_cos(input)), quint32(row["cos"].toInteger()));
        }
        const auto cameras = reference_["cameras"].toArray();
        QCOMPARE(cameras.size(), 72);
        for (const auto &entry : cameras) {
            const auto row = entry.toObject();
            float values[14], matrix[16];
            const auto fields = row["bank"].toArray();
            for (int i = 0; i < 14; ++i)
                values[i] = float(fields[i].toDouble());
            QVERIFY(lordly_graphics_look_at(matrix, values + 5, values + 8, values + 11));
            QCOMPARE(encoded(matrix).toHex(), row["view"].toString().toLatin1());
            QVERIFY(lordly_graphics_perspective(matrix, values[0], values[1], values[2], values[3], values[4]));
            QCOMPARE(encoded(matrix).toHex(), row["projection"].toString().toLatin1());
        }
        const auto rotations = reference_["rotations"].toArray();
        QCOMPARE(rotations.size(), 54);
        for (const auto &entry : rotations) {
            const auto row = entry.toObject();
            const auto angles = row["angles"].toArray();
            float matrix[16];
            lordly_matrix_rotation(matrix, float(angles[0].toDouble()), float(angles[1].toDouble()),
                                   float(angles[2].toDouble()), float(reference_["radians"].toDouble()));
            QCOMPARE(encoded(matrix).toHex(), row["matrix"].toString().toLatin1());
        }
        qInfo() << "Original instruction results matched; host-libm differences in the reference:"
                << reference_["hostDifferences"].toInt();
    }
    void projectionAndPickingUseRecoveredMatrices() {
        if (reference_.isEmpty())
            QSKIP("Private ROM required for original camera matrices");
        float identity[16];
        lordly_matrix_identity(identity);
        QByteArray packed(64, 0);
        QVERIFY(lordly_matrix_pack(reinterpret_cast<uint8_t *>(packed.data()), identity));
        for (const auto &entry : reference_["cameras"].toArray()) {
            const auto row = entry.toObject();
            const auto view = QByteArray::fromHex(row["view"].toString().toLatin1());
            const auto projection = QByteArray::fromHex(row["projection"].toString().toLatin1());
            auto v = [&](int i) { return double(std::bit_cast<float>(u32(view, i * 4))); };
            auto p = [&](int i) { return double(std::bit_cast<float>(u32(projection, i * 4))); };
            const SceneCamera camera(bank(row["bank"].toArray()), 0.1f);
            for (const auto point : {SceneVector{37.25, -18.5, 53.75}, SceneVector{-12, 7.25, -3}}) {
                const auto world = point * camera.modelScale();
                const auto x = world.x * v(0) + world.y * v(4) + world.z * v(8) + v(12);
                const auto y = world.x * v(1) + world.y * v(5) + world.z * v(9) + v(13);
                const auto z = world.x * v(2) + world.y * v(6) + world.z * v(10) + v(14);
                const auto projected = camera.project(point);
                QVERIFY(std::abs(projected.screen.x() - (160 + 160 * x * p(0) / (z * p(11)))) < 1e-8);
                QVERIFY(std::abs(projected.screen.y() - (120 - 120 * y * p(5) / (z * p(11)))) < 1e-8);
                QVERIFY((camera.unprojectAtHeight(projected.screen, point.y) - point).length() < 1e-8);
            }
            // A plane's homography must agree with direct point projection,
            // including rolled/non-unit-up cameras and non-default aspect.
            const SceneVector origin{3, -2, 7};
            const auto plane = camera.projectPackedPlane(packed, origin, {1, 0, 0}, {0, -1, 0}, {8, 8});
            const auto point = camera.projectPackedPoint(packed, origin + SceneVector{2, -3, 0});
            const auto &m = plane.mapping;
            const double w = 2 * m[2] + 3 * m[5] + m[8];
            QVERIFY(std::abs(point.screen.x() - (2 * m[0] + 3 * m[3] + m[6]) / w) < 1e-8);
            QVERIFY(std::abs(point.screen.y() - (2 * m[1] + 3 * m[4] + m[7]) / w) < 1e-8);
        }
    }
    void invalidCamerasAreAtomic() {
        float matrix[16];
        std::fill(std::begin(matrix), std::end(matrix), 123.f);
        const auto before = encoded(matrix);
        const float eye[3] = {0, 0, 120}, target[3] = {0, 0, 0}, up[3] = {0, 1, 0};
        QVERIFY(!lordly_graphics_look_at(matrix, eye, eye, up));
        QCOMPARE(encoded(matrix), before);
        QVERIFY(!lordly_graphics_look_at(matrix, eye, target, target));
        QCOMPARE(encoded(matrix), before);
        QVERIFY(!lordly_graphics_perspective(matrix, 0, 1, 1, 100, 1));
        QVERIFY(!lordly_graphics_perspective(matrix, 45, 0, 1, 100, 1));
        QVERIFY(!lordly_graphics_perspective(matrix, 45, 1, 1, 1, 1));
        QVERIFY(!lordly_graphics_perspective(matrix, 45, 1, 1, 100, 0));
        QVERIFY(!lordly_graphics_perspective(matrix, 1e-40f, 1, 1, 100, 1));
        QCOMPARE(encoded(matrix), before);
        QVERIFY(std::isnan(lordly_graphics_sin(std::numeric_limits<float>::quiet_NaN())));
        QVERIFY(std::isnan(lordly_graphics_cos(std::numeric_limits<float>::quiet_NaN())));
    }
};
QTEST_GUILESS_MAIN(GraphicsMathTests)
#include "graphics_math_tests.moc"
