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
