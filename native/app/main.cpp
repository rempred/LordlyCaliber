#include "app/main_window.h"
#include "app/theme.h"
#include "app/diagnostics.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("LordlyCaliber");
    app.setApplicationVersion("0.5.3");
    app.setOrganizationName("LordlyCaliber");
    lordly::Diagnostics diagnostics;
    lordly::applyEditorTheme(app);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("rom", "ROM to open");
    parser.process(app);
    lordly::MainWindow window;
    window.show();
    if (!parser.positionalArguments().isEmpty())
        window.loadRom(parser.positionalArguments().front());
    return app.exec();
}
