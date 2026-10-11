// Hidden emulator stand-in: records the save visible at boot and flushes on close.
#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <QWidget>
class Session : public QWidget {
    void closeEvent(QCloseEvent *event) override {
        QFile source(qEnvironmentVariable("LORDLY_P64_FIXTURE_FLUSH"));
        if (source.open(QIODevice::ReadOnly)) {
            QThread::msleep(200);
            QFile dest(qEnvironmentVariable("LORDLY_P64_FIXTURE_SAVE"));
            if (dest.open(QIODevice::WriteOnly)) { dest.write(source.readAll()); dest.close(); }
        }
        event->accept();
        QCoreApplication::quit();
    }
};
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "windows");
    QApplication app(argc, argv);
    Session window;
    window.winId(); // Creates a hidden native window that accepts WM_CLOSE.
    const auto logPath = qEnvironmentVariable("LORDLY_P64_FIXTURE_LOG");
    QFile input(qEnvironmentVariable("LORDLY_P64_FIXTURE_SAVE"));
    QFile snapshot(logPath + ".sra");
    if (input.open(QIODevice::ReadOnly) && snapshot.open(QIODevice::WriteOnly)) {
        snapshot.write(input.readAll()); snapshot.close();
    }
    QFile log(logPath);
    if (!log.open(QIODevice::WriteOnly | QIODevice::Append)) return 2;
    log.write(QJsonDocument(QJsonObject{{"pid", double(app.applicationPid())},
        {"args", QJsonArray::fromStringList(app.arguments())}}).toJson(QJsonDocument::Compact) + '\n');
    log.close();
    QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    return app.exec();
}
