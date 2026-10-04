#include "engine/director.h"
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
         {"directors", "Inspect the ROM's Director resource directory."},
         {"scene", "Inspect commands at a Director z64 payload offset (hex).", "offset"},
         {"tools", "N64 compiler directory. Defaults to the portable bundle.", "path"}});
    // Qt's automatic help/error exit may open a Windows dialog when the
    // executable is run with redirected handles. This entry point is a CLI.
    if (!parser.parse(app.arguments())) {
        QTextStream(stderr) << parser.errorText() << '\n';
        return 2;
    }
    const bool help = parser.isSet("help") || parser.isSet("help-all");
    if (help || (!parser.isSet("rom") && !parser.isSet("version"))) {
        QTextStream(stdout) << parser.helpText();
        return help ? 0 : 2;
    }
    if (parser.isSet("version")) {
        QTextStream(stdout) << app.applicationName() << ' ' << app.applicationVersion() << '\n';
        return 0;
    }
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
        if (parser.isSet("directors") || parser.isSet("scene")) {
            const auto directors = lordly::Directors::load(project.rom());
            QJsonObject classes;
            int commandCount = 0;
            for (const auto &r : directors.entries()) {
                const auto key = QString::number(r.program.resourceClass());
                classes[key] = classes[key].toInt() + 1;
                commandCount += int(r.program.commands.size());
            }
            report["directors"] = QJsonObject{{"selectors", directors.selectors().size()},
                                              {"streams", directors.entries().size()},
                                              {"commands", commandCount},
                                              {"resourceClasses", classes}};
            if (parser.isSet("scene")) {
                bool valid = false;
                const auto offset = parser.value("scene").toUInt(&valid, 16);
                if (!valid || offset < 0x594284)
                    throw lordly::Error("Enter a Director z64 payload offset in hexadecimal");
                const auto scene = directors.entries().constFind(offset - 0x594284);
                if (scene == directors.entries().end())
                    throw lordly::Error("No Director stream has that z64 payload offset");
                QJsonArray commands, selectors;
                for (int selector : scene->selectors)
                    selectors.append(selector);
                for (const auto &command : scene->program.commands) {
                    const auto definition = lordly::Directors::grammar().value(command.opcode);
                    QJsonArray operands;
                    for (int i = 0; i < command.operands.size(); ++i)
                        operands.append(QJsonObject{{"role", definition.operands.value(i)},
                                                    {"value", qint64(command.operands[i])}});
                    commands.append(QJsonObject{{"word", command.word},
                                                {"opcode", qint64(command.opcode)},
                                                {"name", definition.name},
                                                {"operands", operands}});
                }
                report["scene"] = QJsonObject{{"name", scene->name},
                                              {"z64PayloadOffset", qint64(offset)},
                                              {"resourceKey", qint64(scene->key)},
                                              {"selectors", selectors},
                                              {"resourceClass", scene->program.resourceClass()},
                                              {"commands", commands}};
            }
        }
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
