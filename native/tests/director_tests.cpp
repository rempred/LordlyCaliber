#include "engine/director.h"
#include "engine/director_session.h"
#include "game/director/control_flow.h"
#include "game/director/dispatch_core.h"
#include "game/director/stream_words.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
static int definition(void *, LordlyDirectorWord word, LordlyDirectorDefinition *out) {
    const auto found = Directors::grammar().constFind(word);
    if (found == Directors::grammar().end())
        return 0;
    *out = {found->words, found->queryWords, found->requiredMode};
    return 1;
}
struct QueryFixture {
    int value = 0, calls = 0;
};
static int queryService(void *context, LordlyDirectorWord opcode, const LordlyDirectorWord *words,
                        int count, int *value) {
    if (opcode != 0x57 || count != 4 || words[3] != 77)
        return 0;
    auto &fixture = *static_cast<QueryFixture *>(context);
    ++fixture.calls;
    *value = fixture.value;
    return 1;
}
class DirectorTests : public QObject {
    Q_OBJECT
  private slots:
    void launchTranslationsAreAtomic() {
        QVector<quint32> words;
        for (int i = 0; i < 256; ++i)
            words << (0x08880000u | i);
        words << 0x08880100u << 0x80000001u;
        const auto original = words;
        DirectorLaunchTranslations values;
        for (int i = 0; i < 255; ++i)
            values.set(i, quint16(i * 257));
        int missing = -1;
        QCOMPARE(lordly_director_translate(words.data(), int(words.size()), values.values.data(),
                                           values.known.data(), &missing),
                 1);
        QCOMPARE(missing, 255);
        QCOMPARE(words, original);
        values.set(255, 0);
        QCOMPARE(lordly_director_translate(words.data(), int(words.size()), values.values.data(),
                                           values.known.data(), &missing),
                 0);
        for (int i = 0; i < 255; ++i)
            QCOMPARE(words[i], quint32(i * 257));
        QCOMPARE(words[255], 0u);
        QCOMPARE(words[256], original[256]);
        QCOMPARE(words[257], original[257]);
    }
    void continuationSessionOwnership() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto bytes = Rom::open(path).bytes();
        int allocation = 0x2700000;
        auto program = [&](std::initializer_list<quint32> words) {
            QByteArray data(words.size() * 4, 0);
            int at = 0;
            for (auto word : words) {
                put32(data, at, word);
                at += 4;
            }
            const auto stored = encodeLzss(data);
            const auto key = allocation - 0x594280;
            put32(bytes, allocation, quint32(stored.size() + 4));
            put32(bytes, allocation + 4, quint32(data.size()));
            bytes.replace(allocation + 8, stored.size(), stored);
            allocation += (int(stored.size()) + 9) & ~1;
            return quint32(key);
        };
        const quint32 root0 = program(
                          {0x99, 0, 0, 0x80000000, 1, 0, 0x80000000, 0x80000001, 0xff000001}),
                      root1 = program({0x80000003, 2, 0x80000001, 0xff000001}),
                      root2 =
                          program({0x80000004, 0x08880005, 0, 0x80000000, 0x80000001, 0xff000001}),
                      child0 = program({0x99, 1, 0, 0x80000000, 0x80000001}),
                      child1 = program({0x9a, 0x80000000, 0x80000001});
        put32(bytes, 0x1f3ca84, 12);
        put32(bytes, 0x1f3ca88, root0);
        put32(bytes, 0x1f3ca8c, root1);
        put32(bytes, 0x1f3ca90, root2);
        put32(bytes, 0x594280 + 0x189099e, 8);
        put32(bytes, 0x594284 + 0x189099e, child0);
        put32(bytes, 0x594288 + 0x189099e, child1);
        const auto resources = Directors::load(Rom::fromBytes(bytes));
        DirectorSession nested(resources, 0, 2);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(nested.resourceKey(), root0);
        QVERIFY(!nested.isContinuation());
        QCOMPARE(nested.state().resume_cursor, 4); // Original parent, not child zero.
        QCOMPARE(nested.state().counter, 0u);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(nested.state().counter, 1u);
        QCOMPARE(nested.state().resume_cursor, 7);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_END);
        QCOMPARE(nested.dispatch(), LORDLY_DIRECTOR_END);
        DirectorSession missing(resources, 1, 2);
        QCOMPARE(missing.dispatch(), LORDLY_DIRECTOR_TRANSFER);
        QVERIFY(missing.reason().contains("translation value 5"));
        QCOMPARE(missing.resourceKey(), root1);
        DirectorLaunchTranslations translations;
        translations.set(5, 0);
        DirectorSession replacement(resources, 1, 2, translations);
        QCOMPARE(replacement.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(replacement.resourceKey(), root2);
        QCOMPARE(replacement.state().resume_cursor, 0);
        QCOMPARE(replacement.dispatch(), LORDLY_DIRECTOR_YIELD);
        QCOMPARE(replacement.state().resume_cursor, 4);
        QCOMPARE(replacement.dispatch(), LORDLY_DIRECTOR_END);
        DirectorSession suspended(resources, 0, 2);
        for (int i = 0; i < 16; ++i) {
            const auto result = suspended.dispatch({}, 1);
            if (result == LORDLY_DIRECTOR_YIELD)
                break;
            QCOMPARE(result, LORDLY_DIRECTOR_BUDGET);
        }
        QCOMPARE(suspended.resourceKey(), root0);
        QCOMPARE(suspended.state().resume_cursor, 4);
    }
    void recoveredDispatcherAgainstMips() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir work;
        QProcess generator;
        generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("director-dispatch-mips.cjs"),
                                 path, work.filePath("reference.json")});
        QVERIFY(generator.waitForFinished(30000));
        const auto errors = generator.readAllStandardError();
        QVERIFY2(generator.exitCode() == 0, errors.constData());
        QFile file(work.filePath("reference.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = QJsonDocument::fromJson(file.readAll()).object()["rows"].toArray();
        QCOMPARE(rows.size(), 32);
        for (const auto &v : rows) {
            const auto row = v.toObject(), initial = row["initial"].toObject(),
                       expected = row["expected"].toObject();
            QVector<LordlyDirectorWord> words;
            for (const auto &w : row["words"].toArray())
                words << quint32(w.toInteger());
            LordlyDirectorDispatch state{};
            state.resume_cursor = initial["resume"].toInt();
            state.mode = initial["mode"].toInt();
            state.counter = quint32(initial["counter"].toInteger());
            state.controller = quint16(initial["controller"].toInt());
            state.barrier_state = quint8(initial["barrierState"].toInt());
            QueryFixture fixture{initial["producer"].toInt(), 0};
            const auto status = lordly_director_dispatch(&state, words.data(), int(words.size()),
                                                         1000, definition, queryService, &fixture);
            const auto name = row["name"].toString();
            QVERIFY2(status == (expected["result"].toInt() == -1 ? LORDLY_DIRECTOR_END
                                                                 : LORDLY_DIRECTOR_YIELD),
                     qPrintable(name));
            QVERIFY2(state.resume_cursor == expected["resume"].toInt(), qPrintable(name));
            QVERIFY2(state.counter == quint32(expected["counter"].toInteger()), qPrintable(name));
            QVERIFY2(state.barrier_state == expected["barrierState"].toInt(), qPrintable(name));
            QVERIFY2(fixture.calls == expected["producerCalls"].toInt(), qPrintable(name));
        }
    }
    void boundedDispatchAndMissingServices() {
        LordlyDirectorDispatch state{};
        state.mode = 2;
        const LordlyDirectorWord camera[] = {0x36, 0, 0, 0, 0, 0, 0, 0, 0x80000001};
        QCOMPARE(lordly_director_dispatch(&state, camera, 9, 100, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_SERVICE);
        QCOMPARE(state.cursor, 0);
        QCOMPARE(state.resume_cursor, 0);
        QCOMPARE(state.counter, 0u);
        state = {};
        state.mode = 2;
        const LordlyDirectorWord counters[] = {1, 2, 1, 2, 0, 0x80000000};
        QCOMPARE(lordly_director_dispatch(&state, counters, 6, 2, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_BUDGET);
        QCOMPARE(state.cursor, 2);
        QCOMPARE(state.counter, 0u);
        QCOMPARE(lordly_director_dispatch(&state, counters, 6, 100, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_YIELD);
        QCOMPARE(state.resume_cursor, 6);
        state = {};
        state.mode = 2;
        const LordlyDirectorWord restart[] = {0xe, 0, 5, 0x80000000};
        QCOMPARE(lordly_director_dispatch(&state, restart, 4, 10, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_BUDGET);
        QCOMPARE(state.cursor, 0);
        const LordlyDirectorWord truncated[] = {0xe, 0};
        state = {};
        state.mode = 2;
        QCOMPARE(lordly_director_dispatch(&state, truncated, 2, 10, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_INVALID);
        const LordlyDirectorWord transfer[] = {0x99, 7, 0, 0x80000000};
        state = {};
        state.mode = 2;
        QCOMPARE(lordly_director_dispatch(&state, transfer, 4, 10, definition, nullptr, nullptr),
                 LORDLY_DIRECTOR_TRANSFER);
        QCOMPARE(state.stream_selector, 7);
        QCOMPARE(state.cursor, 3);
    }
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
        const auto metadata = QJsonDocument::fromJson(catalog.readAll()).object();
        const auto names = metadata["names"].toObject();
        QCOMPARE(directory.continuationSelectors().size(), 5);
        for (const auto &value : metadata["continuations"].toArray()) {
            const auto row = value.toObject();
            const auto key = quint32(row["key"].toInteger());
            QCOMPARE(directory.continuationSelectors()[row["selector"].toInt()], key);
            const auto &entry = directory.continuations()[key];
            QCOMPARE(sha256(entry.program.source),
                     row["decodedSha256"].toString().toLatin1().toLower());
            QCOMPARE(entry.program.encode(), entry.program.source);
        }
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
