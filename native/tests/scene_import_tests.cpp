#include "engine/project.h"
#include "engine/scene_document_compiler.h"
#include "engine/scene_import.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class SceneImportTests : public QObject {
    Q_OBJECT
  private slots:
    void registeredWaitCompatibility() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFINDTESTDATA("browser-wait-scenes.cjs"), path,
                                     work.filePath("scenes.json")});
            QVERIFY(generator.waitForFinished(60000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile file(work.filePath("scenes.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto rows = QJsonDocument::fromJson(file.readAll()).array();
            QCOMPARE(rows.size(), 57);
            SceneDocumentCompiler compiler;
            for (const auto value : rows) {
                const auto row = value.toObject();
                const auto source = DirectorProgram::decode(
                    QByteArray::fromHex(row["source"].toString().toLatin1()));
                const auto actual = compiler.compile(source, row["document"].toObject());
                QVERIFY2(actual == QByteArray::fromHex(row["expected"].toString().toLatin1()),
                         qPrintable(row["id"].toString()));
            }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void authoredAndWipedProject() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 compiler required");
        try {
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFINDTESTDATA("browser-scene-forms.cjs"), path,
                                     work.filePath("forms.json")});
            QVERIFY(generator.waitForFinished(60000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile file(work.filePath("forms.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto input = QJsonDocument::fromJson(file.readAll()).object();
            const auto cases = input["wipes"].toArray();
            QCOMPARE(cases.size(), 1498);
            int accepted = 0;
            for (const auto value : cases) {
                const auto row = value.toObject();
                const auto original = DirectorProgram::decode(
                    QByteArray::fromHex(row["source"].toString().toLatin1()));
                if (row["expected"].isNull()) {
                    QVERIFY_THROWS_EXCEPTION(Error, wipeSceneProgram(original));
                    continue;
                }
                QVERIFY2(wipeSceneProgram(original).source ==
                             QByteArray::fromHex(row["expected"].toString().toLatin1()),
                         qPrintable(row["id"].toString()));
                ++accepted;
            }
            QVERIFY(accepted > 100);
            qInfo() << "Wipe rules agreed for" << accepted << "accepted scenes and"
                    << 1498 - accepted << "refusals";
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            const auto browser = input["project"].toObject();
            project.importLegacy(browser);
            QVERIFY(project.pendingLegacySections().isEmpty());
            const auto id = SceneId::authored("0123456789abcdef0123456789abcdef");
            QCOMPARE(project.scenePrograms()->program(id).source,
                     QByteArray::fromHex(input["authored"].toString().toLatin1()));
            QCOMPARE(project.sceneEvents()->target("event-director:1:b0032"), 319);
            project.saveFolder(work.filePath("saved"));
            auto loaded = Project::fromRom(rom);
            loaded.applyManifest(Project::readProject(work.filePath("saved")));
            const auto built = rebuild(loaded, tools, work.path());
            const auto exportedRom = Rom::fromBytes(built.image), reread = exportedRom;
            const auto reopened = ScenePrograms::load(reread);
            QCOMPARE(reopened.program(id).source, project.scenePrograms()->program(id).source);
            const auto expected = input["expected"].toObject();
            QCOMPARE(expected.size(), 3);
            for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
                const auto ref = SceneDocumentCompiler::catalog()[it.key()].toObject();
                const auto selector = ref["selectors"].toArray()[0].toInt();
                QCOMPARE(reopened.program(reopened.original().selectors()[selector]).source,
                         QByteArray::fromHex(it.value().toString().toLatin1()));
            }
            auto deleted = Project::fromRom(exportedRom);
            deleted.importLegacy({{"format", "ob64-cutscene-project"},
                                  {"schemaVersion", 2},
                                  {"sourceRevision", "us-rev0"},
                                  {"scenes", QJsonArray{}},
                                  {"eventBindings", QJsonArray{}},
                                  {"deletedAuthoredScenes", QJsonArray{id.authoredId}}});
            QVERIFY(deleted.scenePrograms()->authoredScenes().isEmpty());
            QVERIFY(deleted.sceneEvents()->bindings().isEmpty());
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/scene-forms-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            QCOMPARE(SceneEvents::load(Rom::open(candidate)).target("event-director:1:b0032"), 319);
            qInfo().noquote() << "Authored/wiped import candidate:" << candidate;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void browserProjectExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 compiler required");
        try {
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFINDTESTDATA("browser-dialogue-project.cjs"), path,
                                     work.filePath("scene.json")});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile file(work.filePath("scene.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto input = QJsonDocument::fromJson(file.readAll()).object();
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.importLegacy(input["project"].toObject());
            QVERIFY(project.pendingLegacySections().isEmpty());
            const auto key = project.scenePrograms()->original().selectors()[91];
            const auto expected =
                QByteArray::fromHex(input["expectedProgram"].toString().toLatin1());
            QCOMPARE(project.scenePrograms()->program(key).source, expected);
            project.saveFolder(work.filePath("project"));
            auto loaded = Project::fromRom(rom);
            loaded.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(loaded.scenePrograms()->program(key).source, expected);
            const auto built = rebuild(loaded, tools, work.path());
            const auto reopened = ScenePrograms::load(Rom::fromBytes(built.image));
            QCOMPARE(reopened.program(reopened.original().selectors()[91]).source, expected);
            auto text = Dialogues::load(Rom::fromBytes(built.image));
            const auto selector = input["selector"].toInt();
            const auto entries = input["expected"].toArray();
            QCOMPARE(text.entryCount(Dialogues::Table::Primary, selector), entries.size());
            for (int i = 0; i < entries.size(); ++i)
                QCOMPARE(text.entry(Dialogues::Table::Primary, selector, i),
                         QByteArray::fromHex(entries[i].toString().toLatin1()));
            auto restored = *loaded.scenePrograms();
            restored.restore(key);
            loaded.setScenePrograms(restored.json());
            auto again = Project::fromRom(rom);
            again.applyManifest(loaded.manifest());
            QCOMPARE(again.scenePrograms()->program(key).source, restored.program(key).source);
            QVERIFY(again.pendingLegacySections().isEmpty());
            auto missing = loaded.manifest();
            missing.remove("scenePrograms");
            QVERIFY_THROWS_EXCEPTION(Error, again.applyManifest(missing));
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/scene-import-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            const auto exported = ScenePrograms::load(Rom::open(candidate));
            QCOMPARE(exported.program(exported.original().selectors()[91]).source, expected);
            qInfo().noquote() << "Browser scene import candidate:" << candidate;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void browserProgramCompatibility() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFINDTESTDATA("browser-dialogue-project.cjs"), path,
                                     work.filePath("scene.json")});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile file(work.filePath("scene.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto input = QJsonDocument::fromJson(file.readAll()).object();
            const auto document = input["project"]
                                      .toObject()["scenes"]
                                      .toArray()[0]
                                      .toObject()["document"]
                                      .toObject();
            const auto source = DirectorProgram::decode(
                QByteArray::fromHex(input["originalProgram"].toString().toLatin1()));
            SceneDocumentCompiler compiler;
            QCOMPARE(compiler.compile(source, document, 2),
                     QByteArray::fromHex(input["expectedProgram"].toString().toLatin1()));
            auto bad = document, native = bad["native"].toObject();
            auto commands = native["commands"].toArray();
            auto first = commands[0].toObject();
            auto words = first["words"].toArray();
            words[1] = words[1].toInt() + 1;
            first["words"] = words;
            commands[0] = first;
            native["commands"] = commands;
            bad["native"] = native;
            QVERIFY_THROWS_EXCEPTION(Error, compiler.compile(source, bad));
            native = document["native"].toObject();
            commands = native["commands"].toArray();
            commands.removeAt(0);
            native["commands"] = commands;
            bad["native"] = native;
            QVERIFY_THROWS_EXCEPTION(Error, compiler.compile(source, bad));
            bad = document;
            bad["format"] = "invalid";
            QVERIFY_THROWS_EXCEPTION(Error, compiler.compile(source, bad));
            // A failed document must not poison subsequent conversions in the same batch.
            QCOMPARE(compiler.compile(source, document, 2),
                     QByteArray::fromHex(input["expectedProgram"].toString().toLatin1()));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(SceneImportTests)
#include "scene_import_tests.moc"
