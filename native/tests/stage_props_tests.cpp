#include "engine/direct_pose.h"
#include "engine/stage_placements.h"
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
