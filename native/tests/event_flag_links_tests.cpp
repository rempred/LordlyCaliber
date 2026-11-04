#include "engine/event_flag_links.h"
#include "engine/event_flow.h"
#include "engine/project.h"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>
using namespace lordly;
class EventFlagLinksTests:public QObject {
    Q_OBJECT
    static QVector<EventFlagWriter> actions(const QVector<EventFlagWriter> &writers) {
        QVector<EventFlagWriter> result;for(const auto &writer:writers)if(!writer.programReset)result<<writer;return result;
    }
    static QSet<int> offsets(const QVector<EventFlagWriter> &writers) {
        QSet<int> result;for(const auto &writer:writers)result.insert(writer.offset);return result;
    }
  private slots:
    void kerikovLocalWritersAndCurrentEdits() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();const auto &scenes=*project.scenePrograms();
            auto writers=EventFlagLinks::writers(project.rom(),events,scenes,30,200);auto explicitWriters=actions(writers);
            QCOMPARE(explicitWriters.size(),2);QCOMPARE(offsets(explicitWriters),(QSet<int>{0x1cc,0x1f4}));
            // An ON backlink needs no reset scan: disabling lifecycle results
            // preserves every explicit writer and returns no reset records.
            const auto onlyActions=EventFlagLinks::writers(project.rom(),events,scenes,30,200,false);
            QCOMPARE(onlyActions.size(),explicitWriters.size());QCOMPARE(offsets(onlyActions),offsets(explicitWriters));
            for(const auto &writer:onlyActions)QVERIFY(!writer.programReset);
            const auto flow=EventFlow::read(project.rom(),events,scenes,30);
            for(const auto &writer:explicitWriters) {
                QCOMPARE(writer.row,30);QCOMPARE(writer.state,1);QVERIFY(writer.detail.contains("Kerikov"));
                bool navigable=false;for(const auto &event:flow)if(event.entry==writer.entry)
                    for(const auto &action:event.actions)navigable|=action.instructions.contains(writer.offset);
                QVERIFY(navigable);
            }
            bool incomingReset=false;
            for(const auto &writer:writers)if(writer.programReset) {
                QCOMPARE(writer.state,0);QVERIFY(writer.detail.contains("program-reset"));
                if(writer.row!=30){QCOMPARE(u8(events.program(writer.row),writer.offset+1),quint8(30));incomingReset=true;}
            }
            QVERIFY(incomingReset);
            const auto other=actions(EventFlagLinks::writers(project.rom(),events,scenes,31,200));
            QCOMPARE(other.size(),1);QCOMPARE(other.first().row,31);QCOMPARE(other.first().offset,0x110);
            // Retargeting and clearing an existing action must immediately
            // change its backlinks; original research labels cannot override it.
            events.editParameter(30,0x1c8,"value",201,scenes);
            events.editParameter(30,0x1ca,"value",0,scenes);
            explicitWriters=actions(EventFlagLinks::writers(project.rom(),events,scenes,30,200));
            QCOMPARE(offsets(explicitWriters),(QSet<int>{0x1f4}));
            const auto redirected=actions(EventFlagLinks::writers(project.rom(),events,scenes,30,201));bool cleared=false;
            for(const auto &writer:redirected)if(writer.offset==0x1cc){QCOMPARE(writer.state,0);cleared=true;QVERIFY(!writer.title.contains("Kerikov"));}
            QVERIFY(cleared);
            // The current OFF branch now ends the handler before its old
            // setter. The old literal bytes remain present but are unreachable.
            events.editParameter(31,0x104,"destination",0x106,scenes);
            QVERIFY(actions(EventFlagLinks::writers(project.rom(),events,scenes,31,200)).isEmpty());
        }catch(const std::exception &error){QFAIL(error.what());}
    }
    void globalAndNestedAuthoredWriters() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();const auto &scenes=*project.scenePrograms();
            QElapsedTimer timer;timer.start();const auto global=actions(EventFlagLinks::writers(project.rom(),events,scenes,30,179));
            qInfo()<<"Global native flag writer lookup ms"<<timer.elapsed();QSet<int> rows;bool set=false,clear=false;
            for(const auto &writer:global){rows.insert(writer.row);set|=writer.state==1;clear|=writer.state==0;}
            QVERIFY(rows.contains(4));QVERIFY(rows.contains(5));QVERIFY(set);QVERIFY(clear);
            const int flag=events.createFlag("Kerikov rescued by my story");
            const QJsonObject condition{{"type","flag"},{"flag",200},{"value",1}};
            const QJsonObject node{{"type","if"},{"condition",condition},
                {"then",QJsonArray{QJsonObject{{"type","flag"},{"flag",flag},{"value",1}}}},
                {"else",QJsonArray{QJsonObject{{"type","flag"},{"flag",flag},{"value",0}}}}};
            QJsonObject tree{{"id","kerikov-custom"},{"name","Rescue Kerikov"},{"group",0},{"nodes",QJsonArray{node}}};
            events.setTrees(10,{tree},scenes);
            auto custom=actions(EventFlagLinks::writers(project.rom(),events,scenes,30,flag));
            QCOMPARE(custom.size(),2);set=false;clear=false;
            for(const auto &writer:custom) {
                QCOMPARE(writer.row,10);QCOMPARE(writer.authoredId,QString("kerikov-custom"));QCOMPARE(writer.title,QString("Rescue Kerikov"));
                QVERIFY(writer.entry<=writer.offset);QCOMPARE(u16(events.program(10),writer.offset),quint16(0x7160));
                QVERIFY(writer.detail.contains("Kerikov rescued by my story"));set|=writer.state==1;clear|=writer.state==0;
            }
            QVERIFY(set);QVERIFY(clear);
            // Renaming and moving the authored action changes the same lookup.
            events.renameFlag(flag,"Kerikov safe");tree["name"]="Kerikov refuge";
            tree["nodes"]=QJsonArray{QJsonObject{{"type","flag"},{"flag",flag},{"value",0}}};
            events.setTrees(10,{tree},scenes);custom=actions(EventFlagLinks::writers(project.rom(),events,scenes,4,flag));
            QCOMPARE(custom.size(),1);QCOMPARE(custom.first().state,0);QCOMPARE(custom.first().title,QString("Kerikov refuge"));
            QVERIFY(custom.first().detail.contains("Kerikov safe"));
            events.setTrees(10,{},scenes);QVERIFY(EventFlagLinks::writers(project.rom(),events,scenes,30,flag).isEmpty());
        }catch(const std::exception &error){QFAIL(error.what());}
    }
    void originalKnownWriterCensus() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto censusPath=qEnvironmentVariable("LORDLY_FLAG_CENSUS");
        if(censusPath.isEmpty())censusPath=QFINDTESTDATA("../../../docs/event-semantics/flag-index.json");
        if(censusPath.isEmpty()||!QFile::exists(censusPath))QSKIP("Optional parent research census unavailable");
        try {
            QFile file(censusPath);QVERIFY(file.open(QIODevice::ReadOnly));
            const auto census=QJsonDocument::fromJson(file.readAll()).object();QVERIFY(census["flags"].isArray());
            auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();const auto &events=*project.sceneEvents();const auto &scenes=*project.scenePrograms();
            // Independent authenticated census: exact current setter PC, target
            // ID and Boolean result. One scanner pass per program, not one
            // whole-corpus pass per flag. Research data is test-only.
            auto identity=[](int row,int offset,int flag){return QString("%1:%2:%3").arg(row).arg(offset).arg(flag);};
            QMap<QString,int> expected;QSet<int> rows;
            for(const auto &value:census["flags"].toArray()) {
                const auto flag=value.toObject();const int id=flag["id"].toInt();
                for(const auto &siteValue:flag["eventWrites"].toArray()) {
                    const auto site=siteValue.toObject();const int row=site["row"].toInt();rows.insert(row);
                    expected[identity(row,site["offset"].toInt(),id)]=site["value"].toInt()==1?1:0;
                }
            }
            QCOMPARE(expected.size(),947);
            QMap<QString,QSet<int>> found;QElapsedTimer timer;timer.start();
            for(int row:rows)for(const auto &writer:EventFlagLinks::writersInProgram(project.rom(),events,scenes,row)) {
                QVERIFY(!writer.programReset);found[identity(row,writer.offset,writer.flag)].insert(writer.state);
            }
            QStringList missing,wrong;
            for(auto it=expected.begin();it!=expected.end();++it) {
                if(!found.contains(it.key()))missing<<it.key();
                else if(!found[it.key()].contains(it.value()))wrong<<it.key();
            }
            qInfo()<<"Native writer census"<<expected.size()<<"expected;"<<found.size()<<"found; ms"<<timer.elapsed();
            if(!missing.isEmpty())qWarning()<<"Missing row:PC:flag"<<missing;
            if(!wrong.isEmpty())qWarning()<<"Wrong state row:PC:flag"<<wrong;
            QVERIFY2(missing.isEmpty(),"Some authenticated original flag writers are absent from current-code links");
            QVERIFY2(wrong.isEmpty(),"Some authenticated original flag writer values differ from current-code links");
        }catch(const std::exception &error){QFAIL(error.what());}
    }
};
QTEST_GUILESS_MAIN(EventFlagLinksTests)
#include "event_flag_links_tests.moc"
