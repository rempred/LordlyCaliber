#include "engine/director_session.h"
#include "engine/project.h"
#include "engine/scene_actions.h"
#include "rebuild/compiler.h"
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class SceneActionTests : public QObject {
    Q_OBJECT
  private slots:
    static DirectorProgram program(std::initializer_list<quint32> words) {
        QByteArray bytes;
        for (auto word : words) {
            auto at = bytes.size();
            bytes.resize(at + 4);
            put32(bytes, at, word);
        }
        return DirectorProgram::decode(bytes);
    }
    void authoringBoundaries() {
        const auto source =
            program({0x80000006, 1,     0x36, 0,          0,          0,          0,         0,
                     100000,     45000, 0x35, 0,          0,          0,          0,         0,
                     100000,     45000, 0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        const auto points = SceneActions::positions(source);
        QCOMPARE(points.size(), 1);
        QCOMPARE(points[0].command, 3);
        QVERIFY(points[0].canWait);
        for (auto kind : SceneActions::kinds().keys()) {
            const auto inserted = SceneActions::insert(source, 3, kind);
            QCOMPARE(inserted.source.first(72), source.source.first(72));
            QCOMPARE(inserted.source.last(20), source.source.last(20));
            QCOMPARE(SceneActions::remove(inserted, 3).source, source.source);
        }
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(source, 0, 7));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(source, 1, 7));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::remove(source, 1));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(source, 3, 0xe, 0));
        const auto noBarrier = program({0x7d, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(noBarrier).first().canWait);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(noBarrier, 0, 0xe));
        const auto operandYield = program({7, 0, 0x80000000, 0, 0, 0, 1, 0, 0x7d, 0x80000002,
                                           0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(operandYield).first().canWait);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::remove(operandYield, 0));
        const auto branch =
            program({0x80000005, 10, 0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(branch).first().canWait);
        // Shared waits cannot shift deadlines past signed32 or cross unknown control.
        const auto overflow = program(
            {1,   0, 0x80000002, 0x80000000, 0xe,  2,          10,         7,          0,
             0,   0, 0,          0,          1,    0,          0,          0x80000002, 0x80000000,
             0xe, 2, 0x7fffffff, 2,          0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::insert(overflow, 5, 0xe, 1));
        const auto noReset = program(
            {1, 7, 0, 0, 0, 0, 0, 1, 0, 0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(noReset).first().canWait);
        auto branchThenArm = noReset;
        branchThenArm.commands.insert(2, {0, 2, {}});
        branchThenArm = DirectorProgram::decode(branchThenArm.encode());
        QVERIFY(SceneActions::positions(branchThenArm).first().canWait);
        branchThenArm.commands.prepend({0, 0x80000005, {10}});
        branchThenArm = DirectorProgram::decode(branchThenArm.encode());
        QVERIFY(!SceneActions::positions(branchThenArm).first().canWait);
        const auto extraYield = program({1, 0, 0x80000002, 0x80000000, 7, 0, 0, 0, 0, 0, 1, 0, 2,
                                         0x7d, 0x80000002, 0x80000000, 0x80000001, 0xff000005});
        QVERIFY(!SceneActions::positions(extraYield).first().canWait);
    }
    void insertedWaitTimingAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId original;
            for (auto id : scenes.scenes())
                if (scenes.canCreateBlank(id)) {
                    original = id;
                    break;
                }
            QVERIFY(original);
            const auto id = scenes.createScene("Action authoring", original, true);
            const auto blank = scenes.program(id);
            auto standalone = SceneActions::insert(blank, 3, 0x14);
            standalone = SceneActions::insert(standalone, 4, 0xe, 3);
            standalone = SceneActions::insert(standalone, 10, 0xb5, 4);
            const auto span =
                program({1, 0,    0x80000002,     0x80000000,     0xe,        2,         10,
                         7, 0,    quint32(-1000), quint32(-1000), 0,          0,         1,
                         0, 0,    0x80000002,     0x80000000,     0xe,        2,         20,
                         2, 0x7d, 0x80000002,     0x80000000,     0x80000001, 0xff000005});
            auto run = [&](const DirectorProgram &p, int moveAt, int releaseAt) {
                auto current = scenes;
                current.replace(id, p.source);
                DirectorSession session(current.runtimeDirectors(),
                                        current.resource(id).selectors.first(), 0);
                QVector<int> moves;
                int released = -1, update = 0;
                auto service = [&](quint32 opcode, const QVector<quint32> &, int &) {
                    if (opcode == 7)
                        moves << update;
                    if (opcode == 0x7d)
                        released = update;
                    return true;
                };
                QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
                for (update = 1; update <= releaseAt; ++update) {
                    session.advanceCounter();
                    QCOMPARE(session.dispatch(service),
                             update == releaseAt ? LORDLY_DIRECTOR_RELEASE : LORDLY_DIRECTOR_YIELD);
                }
                QCOMPARE(released, releaseAt);
                QCOMPARE(session.state().counter, quint32(0));
                QCOMPARE(moves, moveAt < 0 ? QVector<int>{} : QVector<int>{moveAt});
            };
            run(standalone, -1, 7);
            run(span, 10, 20);
            const auto inserted = SceneActions::insert(span, 5, 0xe, 30);
            QCOMPARE(inserted.commands[8].operands[1], quint32(40));
            QCOMPARE(inserted.commands[13].operands[1], quint32(50));
            run(inserted, 40, 50);
            const auto edited = SceneActions::edit(inserted, 5, 0, "7");
            QCOMPARE(edited.commands[8].operands[1], quint32(17));
            QCOMPARE(edited.commands[13].operands[1], quint32(27));
            run(edited, 17, 27);
            QCOMPARE(SceneActions::remove(edited, 5).source, span.source);
            // Keep real class-five setup, authored creation, sequential waits and native exit.
            scenes.replace(id, standalone.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, standalone.source);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-action-insert-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            const auto rom = Rom::open(destination);
            auto reopened = Project::fromRom(rom);
            reopened.loadScenePrograms();
            QCOMPARE(reopened.scenePrograms()->program(id).source, standalone.source);
            const auto placement = SceneProgramResources::placement(rom);
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(rom.bytes().size()));
            const auto directory = Directors::load(rom);
            const int selector = scenes.resource(id).selectors.first();
            QCOMPARE(directory.entries()[directory.selectors()[selector]].program.source,
                     standalone.source);
            auto checksum = rom.bytes();
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, rom.bytes());
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            qInfo().noquote() << "Rebuilt and reopened" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void exactFields() {
        SceneActionField coordinate{"X", 0, 0, 1000};
        for (const qint32 value : {qint32(-2147483647 - 1), -12375, -1, 0, 1, 12375, 2147483647})
            QCOMPARE(coordinate.parse(coordinate.text(quint32(value))), quint32(value));
        QCOMPARE(coordinate.parse("-0.001"), quint32(-1));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("2147483.648"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("-2147483.649"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("0.0001"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("999999999999999999999.1"));
        QVERIFY_THROWS_EXCEPTION(Error, coordinate.parse("-"));
        QByteArray bytes;
        for (quint32 word : {7U,          0U,          1000U,       quint32(-2500), 4500U,
                             7000U,       30U,         0U,          0x1bU,          30U,
                             12U,         34U,         56U,         0xffffffffU,    255U,
                             1U,          0U,          0x80000002U, 0x80000000U,    0xeU,
                             2U,          30U,         2U,          0x7dU,          0x80000002U,
                             0x80000000U, 0x80000001U, 0xff000005U}) {
            const auto at = bytes.size();
            bytes.resize(at + 4);
            put32(bytes, at, word);
        }
        const auto source = DirectorProgram::decode(bytes);
        const auto actions = SceneActions::inspect(source);
        QCOMPARE(actions.size(), 3);
        QCOMPARE(actions[0].name, QString("Move Actor"));
        QCOMPARE(actions[2].commandCount, 6);
        auto expected = bytes;
        put32(expected, 16, quint32(-12375));
        QCOMPARE(SceneActions::edit(source, 0, 3, "-12.375").source, expected);
        expected = bytes;
        put32(expected, 21 * 4, 91);
        QCOMPARE(SceneActions::edit(source, actions[2].command, 0, "91").source, expected);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, actions[2].command, 0, "0"));
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(source, actions[1].command, 1, "256"));
        auto unknown = bytes;
        put32(unknown, 10 * 4, 0x1234000c); // Noncanonical byte encoding stays intact.
        const auto odd = DirectorProgram::decode(unknown);
        QVERIFY_THROWS_EXCEPTION(Error, SceneActions::edit(odd, 1, 1, "12"));
        auto preserved = unknown;
        put32(preserved, 9 * 4, 50);
        QCOMPARE(SceneActions::edit(odd, 1, 0, "50").source, preserved);
    }
    void waitAndRebuild() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenePrograms();
            const auto base = *project.scenePrograms();
            quint32 key = 0;
            for (const auto &r : base.original().entries())
                if (r.program.resourceClass() == 5) {
                    key = r.key;
                    break;
                }
            QVERIFY(key != 0);
            for (const auto opcode : {0xeU, 0xb5U})
                for (const int compare : {0, 2}) {
                    QByteArray bytes;
                    for (const auto word :
                         {1U, 0U, 0x80000002U, 0x80000000U, opcode, quint32(compare), 7U, 2U, 0x7dU,
                          0x80000002U, 0x80000000U, 0x80000001U, 0xff000005U}) {
                        const auto at = bytes.size();
                        bytes.resize(at + 4);
                        put32(bytes, at, word);
                    }
                    const auto source = DirectorProgram::decode(bytes);
                    for (const int duration : {1, 2, 30, 120}) {
                        const auto edited =
                            SceneActions::edit(source, 0, 0, QString::number(duration));
                        auto expected = bytes;
                        put32(expected, 24, duration);
                        QCOMPARE(edited.source, expected);
                        auto scenes = base;
                        scenes.replace(key, edited.source);
                        DirectorSession session(scenes.runtimeDirectors(),
                                                scenes.resource(key).selectors.first(), 0);
                        bool released = false;
                        auto service = [&](quint32 op, const QVector<quint32> &, int &) {
                            released |= op == 0x7d;
                            return true;
                        };
                        QCOMPARE(session.dispatch(service), LORDLY_DIRECTOR_YIELD);
                        for (int update = 1; update <= duration; ++update) {
                            session.advanceCounter();
                            QCOMPARE(session.dispatch(service), update == duration
                                                                    ? LORDLY_DIRECTOR_RELEASE
                                                                    : LORDLY_DIRECTOR_YIELD);
                            QCOMPARE(released, update == duration);
                        }
                        QCOMPARE(session.state().counter, quint32(0));
                        if (opcode == 0xb5) {
                            DirectorSession skip(scenes.runtimeDirectors(),
                                                 scenes.resource(key).selectors.first(), 0);
                            QCOMPARE(skip.dispatch(service), LORDLY_DIRECTOR_YIELD);
                            skip.setController(0x8000);
                            QCOMPARE(skip.dispatch(service), LORDLY_DIRECTOR_RELEASE);
                            QCOMPARE(skip.state().counter, quint32(0));
                        }
                    }
                }
            // Edit a real movement, preserving every unrelated word in its actual scene.
            auto scenes = base;
            key = 0x1a14970;
            const auto original = scenes.program(key);
            int movement = -1;
            for (const auto &action : SceneActions::inspect(original))
                if (action.name == "Move Actor") {
                    movement = action.command;
                    break;
                }
            QVERIFY(movement >= 0);
            const auto edited = SceneActions::edit(original, movement, 3, "-12.375");
            auto expected = original.source;
            put32(expected, original.commands[movement].word * 4 + 16, quint32(-12375));
            QCOMPARE(edited.source, expected);
            scenes.replace(key, edited.source);
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(key).source, expected);
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-scene-action-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            const auto reopenedRom = Rom::open(destination), sourceAgain = Rom::open(path);
            const auto reopened = Directors::load(reopenedRom);
            for (const auto selector : scenes.resource(key).selectors)
                QCOMPARE(reopened.entries()[reopened.selectors()[selector]].program.source,
                         expected);
            const auto placement = SceneProgramResources::placement(reopenedRom);
            QVERIFY(placement && quint64(placement->offset) + placement->allocation <=
                                     quint64(reopenedRom.bytes().size()));
            auto checksum = reopenedRom.bytes();
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, reopenedRom.bytes());
            QCOMPARE(sourceAgain.bytes(), rom.bytes());
            qInfo().noquote() << "Rebuilt and reopened" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(SceneActionTests)
#include "scene_action_tests.moc"
