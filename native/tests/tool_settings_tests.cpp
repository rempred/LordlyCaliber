#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class ToolSettingsTests : public QObject {
    Q_OBJECT
  private slots:
    void projectPersistenceAndImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        project.loadTools();
        QVERIFY(project.tools()->available());
        QCOMPARE(project.tools()->experiencePercent(), 100);
        project.setTools({{"version", 1}, {"experienceScalePercent", 137}});
        QCOMPARE(project.changedRecords(), 1);
        QTemporaryDir folder;
        project.saveFolder(folder.path());
        project.saveArchive(folder.filePath("tools.lcproject"));
        for (const auto &location : {folder.path(), folder.filePath("tools.lcproject")}) {
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(Project::readProject(location));
            QCOMPARE(reopened.tools()->experiencePercent(), 137);
        }
        const auto saved = project.manifest();
        QVERIFY_THROWS_EXCEPTION(Error, project.setTools({{"version", 1},
                                                          {"experienceScalePercent", 137},
                                                          {"unknownFeature", true}}));
        QCOMPARE(project.manifest(), saved);
        for (const QJsonValue value :
             {QJsonValue(-1), QJsonValue(301), QJsonValue(1.5), QJsonValue("75")}) {
            QVERIFY_THROWS_EXCEPTION(
                Error, project.setTools({{"version", 1}, {"experienceScalePercent", value}}));
            QCOMPARE(project.manifest(), saved);
        }
        const QJsonObject patch{
            {"format", "ob64-patch"},
            {"version", 36},
            {"patches",
             QJsonObject{{"tools", QJsonObject{{"experience-size-scale",
                                                QJsonObject{{"schema", 1}, {"percent", 75}}}}}}}};
        auto imported = Project::fromRom(rom);
        imported.importLegacy(patch);
        QCOMPARE(imported.tools()->experiencePercent(), 75);
        QVERIFY(imported.pendingLegacySections().isEmpty());
        imported.setTools({{"version", 1}, {"experienceScalePercent", 100}});
        auto restored = Project::fromRom(rom);
        restored.applyManifest(imported.manifest());
        QCOMPARE(restored.tools()->experiencePercent(), 100); // Native reset supersedes old import.
        auto owner = patch;
        owner["patches"] = QJsonObject{};
        owner["rebuild_owners"] = QJsonObject{{"schema", 1}, {"experience_scale_percent", 300}};
        imported.importLegacy(owner);
        QCOMPARE(imported.tools()->experiencePercent(), 300);
        QVERIFY(imported.pendingLegacySections().isEmpty());
        auto other = patch;
        auto values = other["patches"].toObject();
        auto settings = values["tools"].toObject();
        settings["character-card-luck"] = true;
        values["tools"] = settings;
        other["patches"] = values;
        imported.importLegacy(other);
        QCOMPARE(imported.pendingLegacySections(), QStringList{"tools.character-card-luck"});
        auto conflict = patch;
        conflict["rebuild_owners"] = QJsonObject{{"experience_scale_percent", 137}};
        const auto before = imported.manifest();
        QVERIFY_THROWS_EXCEPTION(Error, imported.importLegacy(conflict));
        QCOMPARE(imported.manifest(), before);
        auto bytes = rom.bytes();
        bytes[0x2192f8] ^= 1;
        const auto foreign = ToolSettings::load(Rom::fromBytes(bytes));
        QVERIFY(!foreign.available());
        QVERIFY(!foreign.modified());
    }
    void sourceExportAndReopen() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess reference;
            reference.start("node", {QFINDTESTDATA("experience-browser-reference.cjs"), path,
                                     work.filePath("reference.json")});
            QVERIFY(reference.waitForFinished(60000));
            QVERIFY2(reference.exitCode() == 0, reference.readAllStandardError().constData());
            QFile file(work.filePath("reference.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto cases = QJsonDocument::fromJson(file.readAll()).object()["cases"].toArray();
            QCOMPARE(cases.size(), 5);
            QTemporaryDir output(QFileInfo(path).absolutePath() + "/Test ROMs/native-tools-XXXXXX");
            QVERIFY(output.isValid());
            auto project = Project::fromRom(rom);
            for (const auto &v : cases) {
                const auto row = v.toObject();
                const int percent = row["percent"].toInt();
                project.setTools({{"version", 1}, {"experienceScalePercent", percent}});
                const auto built = rebuild(project, tools, work.path());
                auto expected = rom.bytes();
                for (const auto &entry : row["writes"].toArray()) {
                    const auto write = entry.toObject();
                    const auto data = QByteArray::fromHex(write["bytes"].toString().toLatin1());
                    expected.replace(write["offset"].toInt(), data.size(), data);
                }
                QCOMPARE(built.image, expected);
                const auto directory = output.filePath(QString::number(percent));
                QVERIFY(QDir().mkpath(directory));
                const auto candidate = QDir(directory).filePath(QFileInfo(path).fileName());
                exportRom(project, built, candidate);
                project = Project::fromRom(Rom::open(candidate));
                project.loadTools();
                QCOMPARE(project.tools()->experiencePercent(), percent);
                QVERIFY(!project.tools()->modified());
            }
            QCOMPARE(project.rom().bytes(),
                     rom.bytes()); // 100% compiled restoration is byte-exact.
            auto foreign = rom.bytes();
            foreign[0x2192f8] ^= 1;
            auto unchanged = Project::fromRom(Rom::fromBytes(foreign));
            unchanged.loadTools();
            QCOMPARE(rebuild(unchanged, tools, work.path()).image, foreign);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Experience export/reopen candidates:" << output.path();
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
};
QTEST_GUILESS_MAIN(ToolSettingsTests)
#include "tool_settings_tests.moc"
