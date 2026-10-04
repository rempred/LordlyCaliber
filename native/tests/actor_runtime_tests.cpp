#include "engine/actor_motion.h"
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
            const auto fromBits = [](const QJsonValue &value) { return std::bit_cast<float>(quint32(value.toInteger())); };
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
