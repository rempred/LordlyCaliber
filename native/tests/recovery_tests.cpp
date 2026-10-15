#include "engine/recovery_store.h"
#include "engine/project.h"
#include "app/diagnostics.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
using namespace lordly;
class RecoveryTests:public QObject {
    Q_OBJECT
  private slots:
    void rotationAndFailure() {
        QTemporaryDir dir;RecoveryStore store(dir.path(),{12000,4,2,14});
        QJsonObject manifest{{"sourcePath","example.v64"},{"format","LordlyCaliberProject"},{"artwork","original"}};
        QVERIFY_THROWS_EXCEPTION(Error,store.save("../outside",manifest,"Invalid session"));
        QString latest;
        for(int i=0;i<5;++i){manifest["artwork"]=QString::number(i);latest=store.save("one",manifest,"Example");QTest::qWait(2);}
        QCOMPARE(store.entries().size(),2);QCOMPARE(store.load(latest),manifest);
        QFile interrupted(dir.filePath("interrupted.lcrecovery.XYZ123"));QVERIFY(interrupted.open(QIODevice::WriteOnly));
        interrupted.write("incomplete staging file");interrupted.close();
        latest=store.save("one",manifest,"Example");QVERIFY(!interrupted.exists());
        QByteArray random(15000,Qt::Uninitialized);for(auto &byte:random)byte=char(QRandomGenerator::global()->generate());
        auto tooLarge=manifest;tooLarge["artwork"]=QString::fromLatin1(random.toBase64());
        QVERIFY_THROWS_EXCEPTION(Error,store.save("one",tooLarge,"Large"));QCOMPARE(store.load(latest),manifest);
        for(int i=0;i<8;++i){store.save(QString::number(i),manifest,"Other project");QTest::qWait(2);}
        QVERIFY(store.entries().size()<=4);qint64 total=0;
        for(const auto &file:QDir(dir.path()).entryInfoList(QDir::Files))total+=file.size();QVERIFY(total<=12000);
        auto file=store.entries().front().path;QFile corrupt(file);QVERIFY(corrupt.open(QIODevice::Append));corrupt.write("x");corrupt.close();
        QVERIFY_THROWS_EXCEPTION(Error,store.load(file));
        store.remove(file);QVERIFY(!QFile::exists(file));
        const auto next=store.save("keep",manifest,"Kept");store.removeSession("other");QVERIFY(QFile::exists(next));
        store.removeSession("keep");QVERIFY(!QFile::exists(next));
    }
    void projectRecovery() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));
        auto bytes=project.record(Table::Items,1).bytes;bytes[27]=char(0x40);project.replaceBytes(Table::Items,1,bytes);
        project.loadTools();auto settings=project.tools()->json(),rules=settings["gameRules"].toObject();rules["powerUpStrength"]=25;settings["gameRules"]=rules;project.setTools(settings);
        project.loadAnimationRoutes();project.loadAnimationOverrides();
        auto sequences=*project.animationSequences();sequences.create(2,0,*project.animationRoutes());
        project.setAnimationSequences(sequences.json());project.assignPrivateAttack(2,0,4,false,40);
        QTemporaryDir dir;RecoveryStore store(dir.path());const auto manifest=project.manifest();
        const auto saved=store.save("project",manifest,"Example",{"Unapplied form"});
        auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(store.load(saved));
        QCOMPARE(reopened.manifest(),manifest);QCOMPARE(store.entries().front().pending,QStringList{"Unapplied form"});
        QVERIFY(QFileInfo(saved).size()<1024*1024); // No ROM copy in an ordinary edited project.
        auto wrong=Rom::open(path).bytes();wrong[0x1000]^=1;auto mismatch=Project::fromRom(Rom::fromBytes(wrong));
        QVERIFY_THROWS_EXCEPTION(Error,mismatch.applyManifest(store.load(saved)));
    }
    void actualCrashReport() {
#ifdef Q_OS_WIN
        QTemporaryDir dir;QProcess child;
        child.start(QCoreApplication::applicationFilePath(),{"--crash-probe",dir.path()});QVERIFY(child.waitForFinished(30000));
        QVERIFY(child.exitCode()!=0);
        const auto reports=QDir(dir.path()).entryList({"*.crash.txt"},QDir::Files);QCOMPARE(reports.size(),1);
        QFile crash(dir.filePath(reports.front()));QVERIFY(crash.open(QIODevice::ReadOnly));const auto contents=crash.readAll();
        QVERIFY(contents.contains("CRASH:"));QVERIFY(contents.contains("E0424242"));QVERIFY(contents.contains("Module offset:"));
        const auto logs=QDir(dir.path()).entryList({"*.session.log"},QDir::Files);QCOMPARE(logs.size(),1);
        QFile log(dir.filePath(logs.front()));QVERIFY(log.open(QIODevice::ReadOnly));QVERIFY(log.readAll().contains("About to crash for test"));
        for(const auto mode:{"--abort-probe","--terminate-probe"}) {
            QTemporaryDir other;child.start(QCoreApplication::applicationFilePath(),{mode,other.path()});QVERIFY(child.waitForFinished(30000));
            QVERIFY(child.exitCode()!=0);const auto reports=QDir(other.path()).entryList({"*.crash.txt"},QDir::Files);QCOMPARE(reports.size(),1);
            QFile report(other.filePath(reports.front()));QVERIFY(report.open(QIODevice::ReadOnly));QVERIFY(report.readAll().contains("CRASH:"));
        }
#else
        QSKIP("Windows exception reporting");
#endif
    }
    void boundedLogs() {
        QTemporaryDir dir;
        QFile blocked(dir.filePath("blocked"));QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.close();
        {Diagnostics unavailable(blocked.fileName());}QVERIFY(blocked.remove());
        for(int i=0;i<20;++i){QFile f(dir.filePath(QString("old-%1.session.log").arg(i,2,10,QChar('0'))));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");}
        const auto previous=qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &){});
        {Diagnostics logs(dir.path());for(int i=0;i<100;++i)qInfo().noquote()<<QString(16000,'x');}
        qInstallMessageHandler(previous);
        const auto files=QDir(dir.path()).entryInfoList(QDir::Files);
        QVERIFY(files.size()<=15);for(const auto &file:files)QVERIFY(file.size()<=1024*1024);
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);app.setApplicationName("RecoveryTests");app.setApplicationVersion("test");
    if(app.arguments().contains("--crash-probe")||app.arguments().contains("--abort-probe")||app.arguments().contains("--terminate-probe")) {
        Diagnostics logs(app.arguments().last());qInfo()<<"About to crash for test";
        if(app.arguments().contains("--abort-probe"))std::abort();
        if(app.arguments().contains("--terminate-probe"))throw std::runtime_error("Intentional test exception");
#ifdef Q_OS_WIN
        SetErrorMode(SEM_NOGPFAULTERRORBOX);
        RaiseException(0xE0424242,EXCEPTION_NONCONTINUABLE,0,nullptr);
#endif
        return 1;
    }
    RecoveryTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "recovery_tests.moc"
