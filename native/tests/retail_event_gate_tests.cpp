#include "engine/retail_event_gate.h"
#include "engine/event_script.h"
#include "engine/scene_events.h"
#include "engine/event_flag_links.h"
#include "rebuild/retail_event_gate_runtime.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class RetailEventGateTests final : public QObject {
    Q_OBJECT
  private slots:
    void keepBranchAndInsertCondition() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            const auto rom=Rom::open(path);const auto original=SceneEvents::retailRow(rom,10);
            constexpr int pc=0x7e;
            QVERIFY(RetailEventGate::supports(original,10,pc));
            for(auto name:{QString("yes"),QString("no")}) {
                const auto built=RetailEventGate::compile(original,original,10,{{QString::number(pc),QJsonObject{{"mode","keep"},{"outcome",name}}}});
                QCOMPARE(built.size(),original.size());
                QCOMPARE(built.left(pc),original.left(pc));QCOMPARE(built.mid(pc+2),original.mid(pc+2));
                QCOMPARE(u8(built,pc),name=="yes"?1:0);
                QCOMPARE(u8(built,pc+1),name=="yes"?u8(original,pc+1):0);
            }
            const QJsonObject condition{{"type","flag"},{"flag",34},{"value",1}};
            const QJsonObject gate{{"mode","if"},{"condition",condition},{"yes","original"},{"no","no"}};
            const auto built=RetailEventGate::compile(original,original,10,{{QString::number(pc),gate}});
            QCOMPARE(u8(built,pc),8);QVERIFY(built.size()>original.size());
            QCOMPARE(built.mid(2,pc-2),original.mid(2,pc-2));
            QCOMPARE(built.mid(pc+2,original.size()-pc-2),original.mid(pc+2));
            const int table=u16(built,0);QCOMPARE(built.mid(table-1028,4),QByteArray("LCJG"));
            QCOMPARE(RetailEventGate::originalComparison(built,pc),original.size());
            for(const auto &handler:EventScript::decode(built,10))
                for(const auto &step:handler.steps)QVERIFY(step.offset<table-1028);
            QCOMPARE(u8(built,table-1024+u8(built,pc+1)*4+2),1);
            QVERIFY_EXCEPTION_THROWN(RetailEventGate::compile(original,original,10,{{"127",gate}}),Error);
            auto bad=gate;bad["condition"]=QJsonObject{{"type","not-a-condition"}};
            QVERIFY_EXCEPTION_THROWN(RetailEventGate::compile(original,original,10,{{QString::number(pc),bad}}),Error);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void namedConditionReferences() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();
            auto events=*project.sceneEvents();const int flag=events.createFlag("New gate condition");
            const QJsonObject condition{{"type","flag"},{"flag",flag},{"value",1}};
            const QJsonObject gate{{"mode","if"},{"condition",condition},{"yes","original"},{"no","no"}};
            events.setRetailGate(10,0x7e,gate,*project.scenePrograms());
            const auto refs=EventFlagLinks::references(project.rom(),events,*project.scenePrograms(),10,flag);
            QCOMPARE(refs.size(),1);QVERIFY(refs.first().read);QCOMPARE(refs.first().offset,0x7e);
            QVERIFY_EXCEPTION_THROWN(events.removeFlag(flag),Error);
            events.setRetailGate(10,0x7e,{},*project.scenePrograms());
            QVERIFY(EventFlagLinks::references(project.rom(),events,*project.scenePrograms(),10,flag).isEmpty());
            events.removeFlag(flag);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void exportedInstructionFixture() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and N64 compiler required");
        try {
            const auto rom=Rom::open(path);const auto original=SceneEvents::retailRow(rom,10);
            constexpr int pc=0x7e;
            const QJsonObject condition{{"type","flag"},{"flag",34},{"value",1}};
            const QJsonObject gate{{"mode","if"},{"condition",condition},{"yes","original"},{"no","no"}};
            const auto program=RetailEventGate::compile(original,original,10,{{QString::number(pc),gate}});
            QTemporaryDir work;Compiler compiler(tools);
            const auto runtime=buildRetailEventGateRuntime(compiler,work.path(),0x80188000);
            QVERIFY(runtime.entries.value("lc_event_gate_bridge")>=runtime.address);
            auto image=rom.bytes();image.replace(runtime.address-ClassPlacement::residentDelta,runtime.bytes.size(),runtime.bytes);
            linkRetailEventGateRuntime(image,runtime.entries.value("lc_event_gate_bridge"),rom);
            QCOMPARE(u32(image,0x66654),runtime.entries.value("lc_event_gate_bridge"));
            auto unsupported=rom.bytes();put32(unsupported,0x66654,0x80001234);
            QVERIFY_EXCEPTION_THROWN(linkRetailEventGateRuntime(image,0,Rom::fromBytes(unsupported)),Error);
            const auto output=qEnvironmentVariable("LORDLY_GATE_FIXTURE");
            if(!output.isEmpty()) {
                QDir().mkpath(output);
                auto write=[&](const QString &name,const QByteArray &bytes){QFile file(QDir(output).filePath(name));if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size())throw Error("Could not write the gate instruction fixture");};
                write("program.bin",program);write("resident.bin",image.mid(0x39810,0x66e10-0x39810));
                write("gate.json",QJsonDocument(QJsonObject{{"row",10},{"pc",pc},{"yes",pc+2+qint8(u8(original,pc+1))*2},{"no",pc+2},{"flag",34},{"bridge",double(runtime.entries.value("lc_event_gate_bridge"))}}).toJson());
            }
        }catch(const std::exception &e){QFAIL(e.what());}
    }
};
QTEST_APPLESS_MAIN(RetailEventGateTests)
#include "retail_event_gate_tests.moc"
