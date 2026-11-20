#include "app/agent_commands.h"
#include "app/agent_workspace.h"
#include "engine/director.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char **argv) {
    bool workspace = false;
    for (int i = 1; i < argc; ++i)
        workspace |=
            QByteArray(argv[i]) == "--workspace" || QByteArray(argv[i]).startsWith("--workspace=");
    std::unique_ptr<QCoreApplication> app;
    if (workspace) {
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
            qputenv("QT_QPA_PLATFORM", "offscreen");
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        app = std::make_unique<QApplication>(argc, argv);
        if (QGuiApplication::platformName() == "offscreen") {
            // The offscreen plugin does not load the Windows system font database.
            const QDir fonts(QStandardPaths::writableLocation(QStandardPaths::FontsLocation));
            for (const auto name : {"segoeui.ttf", "segoeuib.ttf", "segoeuii.ttf", "georgia.ttf",
                                    "georgiab.ttf", "pala.ttf", "palab.ttf", "consola.ttf"})
                QFontDatabase::addApplicationFont(fonts.filePath(name));
        }
    } else
        app = std::make_unique<QCoreApplication>(argc, argv);
    app->setApplicationName("LordlyCaliber-cli");
    app->setApplicationVersion(LORDLY_VERSION);
    app->setOrganizationName("LordlyCaliber");
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
         {"capabilities", "List structured editor operations and desktop workspace access."},
         {"guide", "Read the packaged CLI guide without a ROM."},
         {"settings-dir", "Use private INI preferences for this desktop workspace.", "folder"},
         {"commands", "Execute an ordered JSON array of editor operations (use - for stdin).",
          "path"},
         {"save-project", "Save the resulting native project folder or .lcproject archive.",
          "path"},
         {"library", "Directory containing packaged game and reference folders.", "path"},
         {"build-plan", "Run the export checks and report active source modules, addresses, "
                        "bindings and source."},
         {"build-dir",
          "Keep compiler inputs, listings, objects, linker scripts and linked code in a new "
          "directory inside this folder.",
          "folder"},
         {"workspace",
          "Open an independent headless desktop session. JSON Lines file or - for interactive "
          "stdin.",
          "path"},
         {"directors", "Inspect the ROM's Director resource directory."},
         {"scene", "Inspect commands at a Director z64 payload offset (hex).", "offset"},
         {"tools", "N64 compiler directory. Defaults to the portable bundle.", "path"}});
    // Qt's automatic help/error exit may open a Windows dialog when the
    // executable is run with redirected handles. This entry point is a CLI.
    if (!parser.parse(app->arguments())) {
        QTextStream(stderr) << parser.errorText() << '\n';
        return 2;
    }
    const bool help = parser.isSet("help") || parser.isSet("help-all");
    if (help || (!parser.isSet("rom") && !parser.isSet("version") &&
                 !parser.isSet("capabilities") && !parser.isSet("guide"))) {
        QTextStream(stdout) << parser.helpText();
        return help ? 0 : 2;
    }
    if (parser.isSet("version")) {
        QTextStream(stdout) << app->applicationName() << ' ' << app->applicationVersion() << '\n';
        return 0;
    }
    if (parser.isSet("capabilities")) {
        QTextStream(stdout)
            << QJsonDocument(
                   QJsonObject{{"protocol", 1},
                               {"operations", lordly::agentCapabilities()},
                               {"page", "Pass page with start and limit to page an array result"},
                               {"guide", "--guide"},
                               {"workspace",
                                "windows, useWindow, pages, openPage, controls, inspect, items, "
                                "menuItems, cell, "
                                "select, edit, activate, "
                                "set, expand, focus, key, pointer, wheel, drop, "
                                "transfer, clipboard, file, reject, image, navigate, save"}})
                   .toJson();
        return 0;
    }
    try {
        if (parser.isSet("settings-dir")) {
            if (!workspace)
                throw lordly::Error("--settings-dir applies to a desktop workspace");
            const auto path = parser.value("settings-dir");
            if (path.isEmpty() || !QDir().mkpath(path))
                throw lordly::Error("Cannot create workspace settings folder");
            QSettings::setDefaultFormat(QSettings::IniFormat);
            QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                               QFileInfo(path).absoluteFilePath());
        }
        if (parser.isSet("guide")) {
            const QDir root(parser.value("library").isEmpty()
                                ? QCoreApplication::applicationDirPath()
                                : parser.value("library"));
            QFile guide(root.filePath("reference/agent-cli.md"));
            if (!guide.open(QIODevice::ReadOnly))
                throw lordly::Error("Missing packaged reference/agent-cli.md");
            QTextStream(stdout) << QString::fromUtf8(guide.readAll());
            return 0;
        }
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
        const auto savePath = parser.value("save-project");
        const bool archive = savePath.endsWith(".lcproject", Qt::CaseInsensitive);
        const auto savedFile = archive ? savePath : QDir(savePath).filePath("project.json");
        QStringList inputs{sourcePath};
        if (parser.isSet("project")) {
            const auto path = parser.value("project");
            inputs << (QFileInfo(path).isDir() ? QDir(path).filePath("project.json") : path);
        }
        if (parser.isSet("commands") && parser.value("commands") != "-") {
            inputs << parser.value("commands");
            if (parser.isSet("save-project") && samePath(savedFile, parser.value("commands")))
                throw lordly::Error("Project destination overlaps the command file");
        }
        for (const auto &input : inputs)
            for (const auto &flag : {QString("output"), QString("report")})
                if (parser.isSet(flag) && samePath(input, parser.value(flag)))
                    throw lordly::Error("Choose an output or report path separate from the inputs");
        if (parser.isSet("save-project") &&
            (savePath.isEmpty() || samePath(savedFile, sourcePath) ||
             (parser.isSet("output") && samePath(savedFile, parser.value("output"))) ||
             (parser.isSet("report") && samePath(savedFile, parser.value("report")))))
            throw lordly::Error("Choose distinct project, report and ROM destinations");
        if (workspace &&
            (parser.isSet("output") || parser.isSet("save-project") || parser.isSet("commands") ||
             parser.isSet("report") || parser.isSet("build-plan") || parser.isSet("build-dir")))
            throw lordly::Error("Use workspace save and desktop export inside a workspace session");
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
            if (manifest["format"].toString() == "LordlyCaliberProject")
                project.applyManifest(manifest);
            else
                project.importLegacy(manifest);
        }
        if (workspace) {
            if (!project.rom().revision()) {
                project.loadScenarios();
                project.loadWorld();
            }
            return lordly::runAgentWorkspace(*static_cast<QApplication *>(app.get()),
                                             std::move(project), parser.value("workspace"));
        }
        auto toolRoot = parser.value("tools");
        if (toolRoot.isEmpty())
            toolRoot = QDir(QCoreApplication::applicationDirPath()).filePath("n64-toolchain");
        QJsonArray commandResults;
        if (parser.isSet("commands")) {
            const auto path = parser.value("commands");
            QFile input(path);
            const bool opened = path == "-" ? input.open(stdin, QIODevice::ReadOnly)
                                            : input.open(QIODevice::ReadOnly);
            if (!opened)
                throw lordly::Error("Cannot read commands: " + input.errorString());
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(input.readAll(), &error);
            if (!doc.isArray())
                throw lordly::Error("Expected a JSON array of commands: " + error.errorString());
            auto library = parser.value("library");
            if (library.isEmpty())
                library = QCoreApplication::applicationDirPath();
            bool edited = false;
            commandResults =
                lordly::runAgentCommands(project, doc.array(), library, toolRoot, edited);
            if (edited && !parser.isSet("save-project") && !parser.isSet("output") &&
                !parser.isSet("build-plan") && !parser.isSet("build-dir"))
                throw lordly::Error("Provide --save-project, --output, or --build-plan to check "
                                    "the requested edits");
        }
        QJsonObject report{
            {"title", project.rom().title()},
            {"revision", project.rom().revision()},
            {"classes", project.records(lordly::Table::Classes).size()},
            {"items", project.records(lordly::Table::Items).size()},
            {"consumables", project.records(lordly::Table::Consumables).size()},
            {"consumableEffectModels",
             project.consumableEffects() ? project.consumableEffects()->models().size() : 0},
            {"shops", project.shops() ? project.shops()->size() - 1 : 0},
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
        if (parser.isSet("output") || parser.isSet("build-plan") || parser.isSet("build-dir")) {
            auto destination = QDir::tempPath();
            if (parser.isSet("build-dir")) {
                destination = QDir(parser.value("build-dir")).absolutePath();
                if (!QDir().mkpath(destination))
                    throw lordly::Error("Cannot create build folder");
            }
            QTemporaryDir build(QDir(destination).filePath("lordly-build-XXXXXX"));
            if (!build.isValid())
                throw lordly::Error("Cannot create temporary export directory");
            if (parser.isSet("build-dir")) {
                build.setAutoRemove(false);
                report["buildDirectory"] = build.path();
                QTextStream(stderr) << "Build directory: " << build.path() << Qt::endl;
            }
            QJsonArray modules, steps;
            auto result = lordly::rebuild(project, toolRoot, build.path(), {},
                                          parser.isSet("build-plan") ? &modules : nullptr,
                                          parser.isSet("build-plan") ? &steps : nullptr);
            if (parser.isSet("build-plan")) {
                report["modules"] = modules;
                report["steps"] = steps;
            }
            if (parser.isSet("output")) {
                lordly::exportRom(project, result, parser.value("output"));
                report["output"] = parser.value("output");
            }
            report["build"] = QJsonArray::fromStringList(result.report);
        }
        if (parser.isSet("save-project")) {
            if (archive)
                project.saveArchive(savePath);
            else
                project.saveFolder(savePath);
            report["savedProject"] = savePath;
        }
        if (parser.isSet("commands"))
            report["results"] = commandResults;
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
