#include "app/main_window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    app.setApplicationName("LordlyCaliber"); app.setApplicationVersion("0.5.0-dev"); app.setOrganizationName("LordlyCaliber");
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette; palette.setColor(QPalette::Window,QColor("#20262e")); palette.setColor(QPalette::WindowText,QColor("#e9edf2"));
    palette.setColor(QPalette::Base,QColor("#171d24")); palette.setColor(QPalette::AlternateBase,QColor("#222a34"));
    palette.setColor(QPalette::Text,QColor("#e9edf2")); palette.setColor(QPalette::Button,QColor("#2b3542"));
    palette.setColor(QPalette::ButtonText,QColor("#e9edf2")); palette.setColor(QPalette::Highlight,QColor("#376a84"));
    palette.setColor(QPalette::HighlightedText,Qt::white); palette.setColor(QPalette::PlaceholderText,QColor("#8d9aa9"));
    app.setPalette(palette); app.setFont(QFont("Segoe UI",10));
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption(); parser.addPositionalArgument("rom","ROM to open"); parser.process(app);
    lordly::MainWindow window; window.show();
    if(!parser.positionalArguments().isEmpty()) window.loadRom(parser.positionalArguments().front());
    return app.exec();
}
