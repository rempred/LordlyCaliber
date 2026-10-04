#include "engine/director.h"
#include "game/director/control_flow.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
using namespace lordly;
class DirectorTests : public QObject {
    Q_OBJECT
  private slots:
    void recoveredControlFlow() {
        // All six comparisons, including equality and signed negative values.
        for (int op = 0; op < 6; ++op)
            for (int value : {-8, -7, -6}) {
                LordlyDirectorWord words[] = {0, quint32(op), quint32(-7), 0x80000002, 0x80000000};
                const bool expected[] = {value == -7, value != -7, value >= -7,
                                         value <= -7, value > -7,  value < -7};
                int nesting = 0, result = -99;
                QCOMPARE(lordly_director_compare(words, 5, 0, value, 0, 0, &nesting, &result), 0);
                QCOMPARE(nesting, expected[op] ? 1 : 0);
                QCOMPARE(result, expected[op] ? 2 : 3);
            }
        LordlyDirectorWord nested[] = {0, 0, 5, 0x80000002, 0, 0x80000002, 0x80000000};
        int nesting = 1, result = -99;
        QCOMPARE(lordly_director_compare(nested, 7, 0, 0, 0, 0, &nesting, &result), 0);
        QCOMPARE(nesting, 1);
        QCOMPARE(result, 3); // Second bridge scanning backward.
        nesting = 2;
        QCOMPARE(lordly_director_compare(nested, 7, 0, 0, 0, 0, &nesting, &result), 0);
        QCOMPARE(nesting, 2);
        QCOMPARE(result, -1); // Barrier found, insufficient bridges.
        QCOMPARE(lordly_director_compare(nested, 7, 0, 0, 0, 1, &nesting, &result), 0);
        QCOMPARE(result, 3); // Resync scans forward, independent of nesting.
        LordlyDirectorWord operandMarker[] = {0, 0, 5, 0x36, 0, 0x80000002, 0x80000000};
        nesting = 0;
        QCOMPARE(lordly_director_compare(operandMarker, 7, 0, 0, 0, 0, &nesting, &result), 0);
        QCOMPARE(result, 5); // Marker inside camera operands must still be consumed.
        result = 42;
        QCOMPARE(lordly_director_compare(nested, 6, 0, 0, 0, 0, &nesting, &result), 1);
        QCOMPARE(result, 42);
        QCOMPARE(nesting, 0);
        QCOMPARE(lordly_director_compare(nested, 7, -1, 0, 0, 0, &nesting, &result), 2);
        LordlyDirectorWord labels[] = {0x36, 0x80000005, 99, 0x80000005, 99};
        QCOMPARE(lordly_director_find_label(labels, 5, 99, &result), 0);
        QCOMPARE(result, 1);
        QCOMPARE(lordly_director_find_label(labels, 5, 98, &result), 1);
        QCOMPARE(lordly_director_find_label(labels, 2, 99, &result), 1);
    }
    void losslessCommandWords() {
        // Environment(-1), camera with signed coordinates, terminal/class 8.
        const auto bytes = QByteArray::fromHex("80000006ffffffff00000036fffffc18000007d000000bb8"
                                               "00000fa0000013880000177000001b5880000001ff000008");
        const auto program = DirectorProgram::decode(bytes);
        QCOMPARE(program.commands.size(), 3);
        QCOMPARE(program.commands[1].word, 2);
        QCOMPARE(program.commands[1].operands[0], quint32(0xfffffc18));
        QCOMPARE(program.resourceClass(), 8);
        QCOMPARE(program.encode(), bytes);
        auto edited = program;
        edited.commands[0].operands[0] = 7;
        auto expected = bytes;
        put32(expected, 4, 7);
        QCOMPARE(edited.encode(), expected);
        QVERIFY_THROWS_EXCEPTION(Error, DirectorProgram::decode(bytes.first(13)));
        QVERIFY_THROWS_EXCEPTION(Error, DirectorProgram::decode(QByteArray::fromHex("00000036")));
        QVERIFY_THROWS_EXCEPTION(Error, DirectorProgram::decode(QByteArray::fromHex("deadbeef")));
        edited.commands[1].operands.removeLast();
        QVERIFY_THROWS_EXCEPTION(Error, edited.encode());
        const auto continuation = DirectorProgram::decode(QByteArray::fromHex("80000001"));
        QCOMPARE(continuation.resourceClass(), -1);
        QCOMPARE(continuation.encode(), QByteArray::fromHex("80000001"));
    }
    void retailDirectoryAndHashes() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private source ROM required");
        const auto rom = Rom::open(path);
        const auto directory = Directors::load(rom);
        QCOMPARE(directory.selectors().size(), 1693);
        QCOMPARE(directory.entries().size(), 1498);
        QFile catalog(":/game/director/grammar.json");
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        const auto names = QJsonDocument::fromJson(catalog.readAll()).object()["names"].toObject();
        int commands = 0;
        QMap<int, int> classes;
        for (const auto &resource : directory.entries()) {
            const auto name = names[QString::number(resource.key)].toObject();
            QVERIFY(!name.isEmpty());
            QCOMPARE(sha256(resource.program.source),
                     name["decodedSha256"].toString().toLatin1().toLower());
            QCOMPARE(resource.program.encode(), resource.program.source);
            for (int selector : resource.selectors)
                QCOMPARE(directory.selectors()[selector], resource.key);
            commands += int(resource.program.commands.size());
            ++classes[resource.program.resourceClass()];
        }
        QCOMPARE(commands, 193771);
        const QMap<int, int> expectedClasses{{1, 369}, {2, 47}, {4, 117}, {5, 92},
                                             {6, 3},   {7, 1},  {8, 869}};
        QCOMPARE(classes, expectedClasses);
        const auto focus = directory.entries().constFind(0x01a14970);
        QVERIFY(focus != directory.entries().end());
        QCOMPARE(focus->envelope + 4, quint32(0x01fa8bf4));
        QCOMPARE(focus->program.commands.size(), 282);
        QCOMPARE(focus->program.resourceClass(), 1);
        auto bad = rom.bytes();
        put32(bad, 0x1f3ca84, 3);
        QVERIFY_THROWS_EXCEPTION(Error, Directors::load(Rom::fromBytes(bad)));
        bad = rom.bytes();
        put32(bad, 0x1f3ca88, 0x10000000);
        QVERIFY_THROWS_EXCEPTION(Error, Directors::load(Rom::fromBytes(bad)));
        bad = rom.bytes();
        put32(bad, 0x1f3e500, 0xffffffff);
        QVERIFY_THROWS_EXCEPTION(Error, Directors::load(Rom::fromBytes(bad)));
        qInfo() << "Preserved" << directory.entries().size() << "streams and" << commands
                << "commands";
    }
};
QTEST_GUILESS_MAIN(DirectorTests)
#include "director_tests.moc"
