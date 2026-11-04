#include "app/scenario_event_editor.h"
#include "engine/event_context.h"
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QTabBar>
#include <QUndoStack>
#include <QtTest>
using namespace lordly;
namespace {
QVector<int> programs(const EventLocationContext &context,int phase){
    QVector<int> rows;for(const auto &choice:context.phases[phase])rows<<choice.row;return rows;
}
}
class EventContextTests:public QObject {
    Q_OBJECT
  private slots:
    void nativeLocationActivities(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();const auto before=project.manifest();
        EventContexts contexts(project.rom(),*project.sceneEvents());const auto tenne=contexts.location(6);
        QCOMPARE(tenne.phases[0].size(),1);QCOMPARE(tenne.phases[0].first().row,0);QCOMPARE(tenne.phases[0].first().entry,0xec);
        QCOMPARE(programs(tenne,1),QVector<int>{4});QCOMPARE(programs(tenne,2),QVector<int>({71,70}));
        QCOMPARE(tenne.phases[2].last().entry,0xa8);QVERIFY(!tenne.phases[1].first().name.startsWith("4 ·"));
        QVERIFY(contexts.locationsForScenario(1).contains(6));QVERIFY(contexts.locationsForProgram(71).contains(6));
        QVERIFY(contexts.locationsForProgram(0,0xf0).contains(6));
        const auto wentinus=programs(contexts.location(27),1);QVERIFY(wentinus.contains(22));QVERIFY(wentinus.contains(23));QVERIFY(wentinus.contains(24));
        QCOMPARE(project.manifest(),before);
    }
    void currentWorldRulesAndScenarioKeys(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();
        const QJsonObject rule{{"conditions",QJsonArray{QJsonObject{{"flag",19},{"value",true}}}},
                               {"event",10},{"clearFrom",200},{"clearTo",255}};
        events.setWorldRules(6,QJsonArray{rule});
        EventContexts conditional(project.rom(),events);const auto withFallback=conditional.location(6);
        QCOMPARE(programs(withFallback,1),QVector<int>({4,10}));QVERIFY(withFallback.phases[0].first().entry>0xec);
        auto always=rule;always["conditions"]=QJsonArray{};events.setWorldRules(6,QJsonArray{always});
        EventContexts unconditional(project.rom(),events);QCOMPARE(programs(unconditional.location(6),1),QVector<int>{10});
        // Editing the actual initialization literal moves the scenario relation
        // without requiring a new hard-coded row-to-scenario association.
        const auto bytes=events.program(10);int initialization=-1;
        for(int at=0;at+6<=bytes.size();at+=2)if(u8(bytes,at)==0x68&&bytes.mid(at+2,4)==QByteArray::fromHex("70e970e7")){initialization=at;break;}
        QVERIFY(initialization>=0);events.editParameter(10,initialization,"value",1,*project.scenePrograms());
        EventContexts edited(project.rom(),events);QVERIFY(edited.locationsForScenario(1).contains(6));
        QVERIFY(edited.scenarioPrograms(1).contains(10));
    }
    void chooseActivitiesAndFollowPrograms(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();const auto before=project.manifest();QUndoStack undo;
        ScenarioEventEditor editor(&project,&undo,[]{});editor.showAllRows();editor.selectLocation(6);editor.show();QCoreApplication::processEvents();
        auto phases=editor.findChild<QTabBar *>("scenarioEventPhase");auto rows=editor.findChild<QComboBox *>("scenarioEventRow");
        auto all=editor.findChild<QCheckBox *>("scenarioEventOtherPrograms");QVERIFY(phases&&rows&&all);QVERIFY(!all->isChecked());
        QCOMPARE(phases->tabText(0),QString("Arrival"));QCOMPARE(phases->tabText(1),QString("Mission"));QCOMPARE(phases->tabText(2),QString("Area Investigation"));
        phases->setCurrentIndex(0);QCOMPARE(rows->currentData().toInt(),0);QCOMPARE(rows->currentData(Qt::UserRole+1).toInt(),0xec);
        editor.setRawInstructions(true);editor.setRawInstructions(false);QCOMPARE(rows->currentData(Qt::UserRole+1).toInt(),0xec);
        phases->setCurrentIndex(1);QCOMPARE(rows->currentData().toInt(),4);
        phases->setCurrentIndex(2);QCOMPARE(rows->currentData().toInt(),71);
        editor.selectEventRow(70,0xa8);QCOMPARE(phases->currentIndex(),2);QCOMPARE(rows->currentData(Qt::UserRole+1).toInt(),0xa8);QVERIFY(!all->isChecked());
        editor.selectEventRow(71);QCOMPARE(rows->currentData().toInt(),71);QVERIFY(!all->isChecked());
        editor.selectEventRow(67);QVERIFY(all->isChecked());QCOMPARE(rows->currentData().toInt(),67);
        all->setChecked(false);QCOMPARE(phases->currentIndex(),2);QCOMPARE(rows->currentData().toInt(),71);
        QCOMPARE(project.manifest(),before);QVERIFY(undo.isClean());
    }
};
QTEST_MAIN(EventContextTests)
#include "event_context_tests.moc"
