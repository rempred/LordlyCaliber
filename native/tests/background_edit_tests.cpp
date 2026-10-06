#include "engine/project.h"
#include "engine/scene_background_edit.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class BackgroundEditTests : public QObject {
    Q_OBJECT
  private slots:
    void choicesAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            QMap<int, quint32> sourceKeys{{1, 0x1a14970}};
            for (const auto &scene : scenes.original().entries())
                if ((scene.program.resourceClass() == 4 || scene.program.resourceClass() == 5 ||
                     scene.program.resourceClass() == 8) &&
                    !sourceKeys.contains(scene.program.resourceClass()) &&
                    BackgroundEdit::inspect(rom, scene.program).editable())
                    sourceKeys[scene.program.resourceClass()] = scene.key;
            QCOMPARE(sourceKeys.size(), 4);
            QJsonArray scans;
            QMap<int, QByteArray> expectedPrograms;
            for (auto it = sourceKeys.begin(); it != sourceKeys.end(); ++it) {
                const auto before = scenes.program(it.value());
                QCOMPARE(before.resourceClass(), it.key());
                const auto edit = BackgroundEdit::inspect(rom, before);
                QVERIFY2(edit.editable(), qPrintable(edit.reason));
                const auto choices = edit.choices(rom);
                QVERIFY(choices.size() > 10);
                if (edit.route == BackgroundEdit::Environment) {
                    QCOMPARE(choices.size(), 63);
                    QCOMPARE(choices.last().selector, 62);
                }
                int changedTo = -1;
                for (const auto &choice : choices) {
                    const auto after = edit.select(rom, before, choice.selector);
                    auto expected = before.source;
                    put32(expected, before.commands[edit.command].word * 4 + 4,
                          quint32(choice.selector));
                    QCOMPARE(after.source, expected); // Every other source byte, including
                                                      // Actors/cameras, is identical.
                    QCOMPARE(SceneImages::launch(after).environment, qint16(choice.selector));
                    auto resource = scenes.resource(it.value());
                    resource.program = after;
                    QCOMPARE(SceneImages::backgrounds(rom, resource).mode,
                             edit.route == BackgroundEdit::SceneGroup ? 0 : 2);
                    if (edit.route == BackgroundEdit::Environment && choice.selector == 57)
                        QVERIFY(choice.images.contains("archive:49"));
                    if (edit.route == BackgroundEdit::Environment && choice.selector == 51)
                        QVERIFY(choice.images.contains("archive:45"));
                    QJsonArray words;
                    for (int at = 0; at < after.source.size(); at += 4)
                        words << double(u32(after.source, at));
                    scans << QJsonObject{{"words", words}, {"selector", choice.selector}};
                    if (choice.selector != edit.selector)
                        changedTo = choice.selector;
                }
                QVERIFY(changedTo >= 0);
                QVERIFY_THROWS_EXCEPTION(Error, edit.select(rom, before, 65535));
                const auto after = edit.select(rom, before, changedTo);
                scenes.replace(it.value(), after.source);
                expectedPrograms[scenes.resource(it.value()).selectors.first()] = after.source;
                auto bad = before;
                bad.commands[edit.command].operands[0] = 0xffffffffU;
                QVERIFY(!BackgroundEdit::inspect(rom, DirectorProgram::decode(bad.encode()))
                             .editable());
                bad = before;
                bad.commands.insert(edit.command + 1, bad.commands[edit.command]);
                QVERIFY(!BackgroundEdit::inspect(rom, DirectorProgram::decode(bad.encode()))
                             .editable());
                bad = before;
                bad.commands.removeAt(edit.command);
                QVERIFY(!BackgroundEdit::inspect(rom, DirectorProgram::decode(bad.encode()))
                             .editable());
                bad = before;
                bad.commands.last().operands[0] = 0xff000002;
                QVERIFY(!BackgroundEdit::inspect(rom, DirectorProgram::decode(bad.encode()))
                             .editable());
            }
            auto changedCode = rom.bytes();
            changedCode[0x4ed60] ^= 1;
            QVERIFY(
                !BackgroundEdit::inspect(Rom::fromBytes(changedCode), scenes.program(sourceKeys[1]))
                     .editable());
            project.setScenePrograms(scenes.json());
            QTemporaryDir work;
            atomicWrite(work.filePath("selections.json"), QJsonDocument(scans).toJson());
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("image-launch-mips.cjs"), path,
                                     work.filePath("scan.json"), work.filePath("selections.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("scan.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(
                QJsonDocument::fromJson(file.readAll()).object()["backgroundSelections"].toInt(),
                int(scans.size()));
            qInfo("Compared %d background choices with original MIPS launch scanning",
                  int(scans.size()));
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->json(), scenes.json());
            const auto built = rebuild(project, tools, work.filePath("compile"));
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-background-choice-XXXXXX");
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto destination = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, destination);
            const auto reopenedRom = Rom::open(destination);
            const auto reopened = Directors::load(reopenedRom);
            for (auto it = expectedPrograms.begin(); it != expectedPrograms.end(); ++it) {
                const auto &resource = reopened.entries()[reopened.selectors()[it.key()]];
                QCOMPARE(resource.program.source, it.value());
                QVERIFY(BackgroundEdit::inspect(reopenedRom, resource.program).editable());
            }
            const auto placement = SceneProgramResources::placement(reopenedRom);
            QVERIFY(placement.has_value());
            QVERIFY(quint64(placement->offset) + placement->allocation <=
                    quint64(reopenedRom.bytes().size()));
            auto checksum = reopenedRom.bytes();
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, reopenedRom.bytes());
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            qInfo().noquote() << "Rebuilt and reopened" << destination;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(BackgroundEditTests)
#include "background_edit_tests.moc"
