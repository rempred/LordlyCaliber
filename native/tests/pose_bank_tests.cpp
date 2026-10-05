#include "engine/actor_art.h"
#include "engine/direct_descriptors.h"
#include "engine/pose_bank.h"
#include "game/director/actor_pose.h"
#include <QtTest>
using namespace lordly;
namespace {
QByteArray bank(const QVector<QByteArray> &programs) {
    QByteArray bytes(programs.size() * 4, 0);
    for (int i = 0; i < programs.size(); ++i) {
        put32(bytes, i * 4, quint32(bytes.size()));
        bytes += programs[i];
    }
    return bytes;
}
int lookup(void *context, const LordlyActorPose *actor, LordlyPoseRecord *record) {
    const auto &program = static_cast<const PoseBank *>(context)->effective(actor->state);
    const auto row = actor->cursor >= 0 && actor->cursor < program.records.size()
                         ? program.records[actor->cursor]
                         : PoseRecord{};
    record->opcode = quint8(row.opcode);
    for (int i = 0; i < row.operands.size(); ++i)
        record->operands[i] = u8(row.operands, i);
    return 1;
}
QVector<int> frames(const PoseBank &bank, int state, int ticks) {
    LordlyActorPose actor{};
    actor.state = qint16(state);
    actor.cursor = -1;
    LordlyPoseInvocation invocation{};
    QVector<int> result;
    for (int i = 0; i < ticks; ++i) {
        if (lordly_actor_advance_pose(&actor, &invocation, 256, lookup, nullptr,
                                      const_cast<PoseBank *>(&bank)) != LORDLY_POSE_OK)
            throw Error("Test pose did not progress");
        result << actor.frame;
    }
    return result;
}
QByteArray resource(const Rom &rom, quint32 key, bool compressed) {
    const auto at = key + 0x594280;
    const auto size = u32(rom.bytes(), at);
    return compressed ? decodeLzss(rom.bytes().mid(at + 8, size - 4), u32(rom.bytes(), at + 4))
                      : rom.bytes().mid(at + 4, size);
}
} // namespace
class PoseBankTests : public QObject {
    Q_OBJECT
  private slots:
    void appendPrograms() {
        const auto original = QByteArray::fromHex("02010a020400");
        auto poses = PoseBank::parse(bank({{}, original, QByteArray::fromHex("0100")}));
        const auto before = poses;
        QCOMPARE(poses.appendProgram(QByteArray::fromHex("02152c01020400")), 3);
        QCOMPARE(frames(poses, 3, 3), (QVector<int>{300, 300, 300}));
        for (int i = 0; i < before.programs().size(); ++i) {
            QCOMPARE(poses.programs()[i].source, before.programs()[i].source);
            QCOMPARE(frames(poses, i, 3), frames(before, i, 3));
        }
        QCOMPARE(PoseBank::parse(poses.encoded()).encoded(), poses.encoded());
        const auto safe = poses.encoded();
        for (const auto &bad : {QByteArray{}, QByteArray::fromHex("010400"),
                               QByteArray::fromHex("0105ff"), QByteArray::fromHex("0116")})
            QVERIFY_THROWS_EXCEPTION(Error, poses.appendProgram(bad));
        QCOMPARE(poses.encoded(), safe);
        auto trailing = PoseBank::parse(bank({original, {}}));
        const auto untouched = trailing.encoded();
        QVERIFY_THROWS_EXCEPTION(Error, trailing.appendProgram(original));
        QCOMPARE(trailing.encoded(), untouched);
        auto limit = PoseBank::parse(bank(QVector<QByteArray>(32768, QByteArray(1, 0))));
        QVERIFY_THROWS_EXCEPTION(Error, limit.appendProgram(original));
    }
    void frameEditingAndAliases() {
        const auto original = QByteArray::fromHex("040c010000010a040114020401");
        const auto bytes = bank({{}, original, QByteArray::fromHex("0100")});
        auto edited = PoseBank::parse(bytes);
        QCOMPARE(edited.encoded(), bytes);
        QCOMPARE(edited.effective(0).source, original);
        QCOMPARE(frames(edited, 1, 6), (QVector<int>{10, 10, 20, 10, 10, 20}));
        edited.setFrame(1, 1, 300, 2);
        QCOMPARE(edited.programs()[0].source, original);
        QCOMPARE(edited.programs()[1].records[1].opcode, 21);
        QCOMPARE(edited.programs()[1].records[1].operands, QByteArray::fromHex("2c0102"));
        QCOMPARE(frames(edited, 0, 6), (QVector<int>{10, 10, 20, 10, 10, 20}));
        QCOMPARE(frames(edited, 1, 5), (QVector<int>{300, 20, 300, 20, 300}));
        edited.insertFrame(1, 2, 30, 2);
        QCOMPARE(frames(edited, 1, 6), (QVector<int>{300, 30, 20, 300, 30, 20}));
        edited.moveFrame(1, 2, 3);
        QCOMPARE(frames(edited, 1, 6), (QVector<int>{300, 20, 30, 300, 20, 30}));
        edited.moveFrame(1, 1, 3);
        QCOMPARE(u8(edited.effective(1).records.last().operands, 0), 3);
        QCOMPARE(frames(edited, 1, 6), (QVector<int>{20, 30, 300, 300, 300, 300}));
        const auto safe = edited.encoded();
        QVERIFY_THROWS_EXCEPTION(Error, edited.removeFrame(1, 3));
        QCOMPARE(edited.encoded(), safe);
        edited.removeFrame(1, 1);
        QCOMPARE(frames(edited, 1, 5), (QVector<int>{30, 300, 300, 300, 300}));
        edited.setFrame(1, 2, 7, 2);
        QCOMPARE(edited.effective(1).records[2].opcode, 21);
        QCOMPARE(edited.effective(1).records[2].operands, QByteArray::fromHex("070002"));
        const auto retained = edited.encoded();
        QVERIFY_THROWS_EXCEPTION(Error, edited.setFrame(1, 2, 65536, 2));
        QVERIFY_THROWS_EXCEPTION(Error, edited.setFrame(1, 0, 1, 2));
        QVERIFY_THROWS_EXCEPTION(Error, edited.setProgram(1, QByteArray::fromHex("010400")));
        QCOMPARE(edited.encoded(), retained);
        auto count = PoseBank::parse(bank({QByteArray::fromHex("0100")}));
        for (int i = 0; i < 254; ++i)
            count.insertFrame(0, 0, i, 0);
        QCOMPARE(count.effective(0).records.size(), 255);
        const auto maximum = count.encoded();
        QVERIFY_THROWS_EXCEPTION(Error, count.insertFrame(0, 0, 1, 2));
        QCOMPARE(count.encoded(), maximum);
        auto aliased = PoseBank::parse(bytes);
        aliased.setFrame(0, 1, 4, 2);
        QCOMPARE(aliased.effective(1).source, original);
        QCOMPARE(frames(aliased, 0, 3), (QVector<int>{4, 20, 4}));
        // Unreachable original control records are preserved, not treated as executed.
        auto unreachable = PoseBank::parse(bank({QByteArray::fromHex("0404020401010a020402")}));
        unreachable.setFrame(0,2,20,2);
        QCOMPARE(frames(unreachable,0,3),(QVector<int>{20,20,20}));
        // Cross-state no-delay cycles and missing states cannot enter authored data.
        auto transitions =
            PoseBank::parse(bank({QByteArray::fromHex("0100"), QByteArray::fromHex("010500")}));
        const auto prior = transitions.encoded();
        QVERIFY_THROWS_EXCEPTION(Error, transitions.setProgram(0, QByteArray::fromHex("010501")));
        QCOMPARE(transitions.encoded(), prior);
        QVERIFY_THROWS_EXCEPTION(Error, transitions.setProgram(0, QByteArray::fromHex("0105ff")));
        QVERIFY_THROWS_EXCEPTION(Error, PoseBank::parseProgram(QByteArray::fromHex("0116")));
        QVERIFY_THROWS_EXCEPTION(Error, PoseBank::parseProgram(QByteArray::fromHex("010100")));
        QVERIFY_THROWS_EXCEPTION(Error, PoseBank::parseProgram(QByteArray::fromHex("0000")));
    }
    void currentRomRoundTrip() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            QSet<quint32> keys;
            for (auto key : DirectDescriptors::load(rom).keys())
                if (key)
                    keys.insert(key);
            const auto cutscenes = resource(rom, 0x109f95e, false);
            for (int i = 0; i < cutscenes.size(); i += 4)
                keys.insert(u32(cutscenes, i));
            int programs = 0, records = 0;
            for (auto key : keys) {
                const auto descriptor = resource(rom, key, false);
                const auto source = resource(rom, u32(descriptor, 4), true);
                const auto poses = PoseBank::parse(source);
                QCOMPARE(poses.encoded(), source);
                programs += int(poses.programs().size());
                for (const auto &program : poses.programs()) {
                    records += int(program.records.size());
                    if (!program.source.isEmpty())
                        QCOMPARE(PoseBank::encodeProgram(program.records), program.source);
                }
            }
            qInfo() << keys.size() << "descriptors," << programs << "physical pose programs,"
                    << records << "instructions round-trip byte-for-byte";
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(PoseBankTests)
#include "pose_bank_tests.moc"
