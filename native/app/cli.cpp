#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("LordlyCaliber-cli");
    app.setApplicationVersion("0.5.0-dev");
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Inspect or rebuild an Ogre Battle 64 project without the desktop UI.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions(
        {{{"r", "rom"}, "Source ROM (read-only).", "path"},
         {{"p", "project"}, "Project manifest, folder or archive to apply.", "path"},
         {{"o", "output"}, "Export destination. Omit to inspect only.", "path"},
         {"report", "Write inspection/build results as JSON.", "path"},
         {"tools", "N64 compiler directory. Defaults to the portable bundle.", "path"}});
    parser.process(app);
    if (!parser.isSet("rom"))
        parser.showHelp(2);
    try {
        const auto sourcePath = QFileInfo(parser.value("rom")).canonicalFilePath();
        auto samePath = [](const QString &a, const QString &b) {
            QFileInfo first(a), second(b);
            return first.absoluteFilePath().compare(second.absoluteFilePath(),
                                                    Qt::CaseInsensitive) == 0 ||
                   (!first.canonicalFilePath().isEmpty() &&
                    first.canonicalFilePath() == second.canonicalFilePath());
        };
        if (parser.isSet("report") &&
            (samePath(parser.value("report"), sourcePath) ||
             (parser.isSet("output") && samePath(parser.value("report"), parser.value("output")))))
            throw lordly::Error("Choose a report path separate from the source and exported ROM");
        QFile catalog(":/catalog.json");
        if (!catalog.open(QIODevice::ReadOnly))
            throw lordly::Error("Missing record catalog");
        auto project =
            lordly::Project::fromRom(lordly::Rom::open(parser.value("rom")),
                                     QJsonDocument::fromJson(catalog.readAll()).object());
        project.loadTexts();
        if (project.rom().revision() == 0) {
            project.loadPromotion();
            project.loadShops();
            project.loadConsumableEffects();
            project.loadEncounters();
        }
        if (parser.isSet("project")) {
            const auto manifest = lordly::Project::readProject(parser.value("project"));
            if (manifest["format"].toString() == "ob64-patch")
                project.importLegacy(manifest);
            else
                project.applyManifest(manifest);
        }
        QJsonObject report{
            {"title", project.rom().title()},
            {"revision", project.rom().revision()},
            {"classes", project.records(lordly::Table::Classes).size()},
            {"items", project.records(lordly::Table::Items).size()},
            {"consumables", project.records(lordly::Table::Consumables).size()},
            {"consumableEffectModels",
             project.consumableEffects() ? project.consumableEffects()->models().size() : 0},
            {"shops", project.shops() ? lordly::Shops::count - 1 : 0},
            {"encounterPools", project.encounters() ? lordly::Encounters::poolCount : 0},
            {"changedEntries", project.changedRecords()},
            {"pendingImportedSections",
             QJsonArray::fromStringList(project.pendingLegacySections())}};
        if (parser.isSet("output")) {
            auto root = parser.value("tools");
            if (root.isEmpty())
                root = QDir(QCoreApplication::applicationDirPath()).filePath("n64-toolchain");
            QTemporaryDir build;
            if (!build.isValid())
                throw lordly::Error("Cannot create temporary export directory");
            auto result = lordly::rebuild(project, root, build.path());
            lordly::exportRom(project, result, parser.value("output"));
            report["output"] = parser.value("output");
            report["build"] = QJsonArray::fromStringList(result.report);
        }
        auto output = QJsonDocument(report).toJson();
        if (parser.isSet("report"))
            lordly::atomicWrite(parser.value("report"), output);
        QTextStream(stdout) << output;
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << Qt::endl;
        return 1;
    }
}
