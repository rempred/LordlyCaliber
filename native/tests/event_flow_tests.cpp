#include "app/scenario_event_editor.h"
#include "app/event_flow_editor.h"
#include "app/theme.h"
#include "engine/event_flow.h"
#include "rebuild/compiler.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QListWidget>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QJsonArray>
#include <QTreeWidget>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest>
using namespace lordly;
class EventFlowTests:public QObject {
    Q_OBJECT
    static const StoryEvent &entry(const QVector<StoryEvent> &events,int at){for(const auto &e:events)if(e.entry==at)return e;throw Error("Missing story event");}
  private slots:
    void initTestCase(){QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");qApp->setFont(QFont("Segoe UI",10));applyEditorTheme(*qApp);}
    void everyInstructionAccountedFor(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();const auto baseline=project.manifest();
            int words=0,programs=0,actions=0,advanced=0,visits=0;
            for(int row=0;row<115;++row){if(row==2||row==3)continue;
                const auto flow=EventFlow::read(project.rom(),*project.sceneEvents(),*project.scenePrograms(),row);
                QSet<int> seen;auto account=[&](int at){if(seen.contains(at))throw Error(QString("Repeated word row %1 @%2").arg(row).arg(at));seen.insert(at);};
                for(const auto &e:flow){for(int at:e.triggerInstructions)account(at);for(const auto &r:e.requirements)for(int at:r.instructions)account(at);visits+=e.simpleVisit;
                    for(const auto &a:e.actions){for(int at:a.instructions)account(at);++actions;advanced+=a.advanced;}}
                const auto expected=EventScript::instructionOffsets(row);
                QVERIFY2(seen==expected,qPrintable(QString("Inventory differs in row %1: expected %2, found %3").arg(row).arg(expected.size()).arg(seen.size())));
                words+=seen.size();++programs;
            }
            QCOMPARE(words,55471);QCOMPARE(programs,113);QCOMPARE(project.manifest(),baseline);QVERIFY(visits>100);
            qInfo()<<"Programs"<<programs<<"words"<<words<<"actions"<<actions<<"advanced blocks"<<advanced<<"simple visits"<<visits;
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void simpleVisitAndExtraLogic(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();
            auto flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),4);auto visit=entry(flow,0xa8);
            QVERIFY(visit.simpleVisit);QCOMPARE(visit.triggerFields.size(),1);QCOMPARE(visit.triggerFields[0].value,2);QCOMPARE(visit.triggerFields[0].choices[2],QString("Mulsuk"));
            QCOMPARE(visit.actions.size(),1);QCOMPARE(visit.actions[0].scene,610);QCOMPARE(visit.triggerInstructions.size()+visit.actions[0].instructions.size(),9);
            const auto original=events.program(4);
            EventFlow::set(events,*project.scenePrograms(),4,visit.triggerFields[0],3);
            EventFlow::set(events,*project.scenePrograms(),4,visit.actions[0].fields[0],611);
            auto expected=original;expected[0xab]=3;expected[0xb1]=char(611&255);QCOMPARE(events.program(4),expected);
            flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),4);visit=entry(flow,0xa8);
            QVERIFY(visit.simpleVisit);QCOMPARE(visit.title,QString("Jadd"));QCOMPARE(visit.actions[0].scene,611);
            // A different return request is an extra action, never silently hidden.
            events.editParameter(4,0xb6,"value",3,*project.scenePrograms());
            flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),4);visit=entry(flow,0xa8);
            QVERIFY(!visit.simpleVisit);QCOMPARE(visit.actions.size(),2);QVERIFY(visit.actions.last().title.contains("world map"));
            // Redirecting the town guard changes its meaning. It must stop being
            // presented as a simple town trigger; its explicit branch remains visible.
            events.editParameter(4,0xac,"destination",0xb6,*project.scenePrograms());
            flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),4);visit=entry(flow,0xa8);
            QVERIFY(visit.triggerFields.isEmpty());QCOMPARE(visit.actions.first().kind,QString("condition"));
            QCOMPARE(visit.actions.first().target,0xb6);QCOMPARE(visit.actions.first().otherwise,0xae);
            const auto gunther=EventFlow::read(project.rom(),*project.sceneEvents(),*project.scenePrograms(),10);
            const auto offer=entry(gunther,74);QCOMPARE(offer.triggerFields.first().value,6);
            QCOMPARE(offer.requirements.size(),2);QCOMPARE(offer.requirements.first().condition.children.size(),2);
            QCOMPARE(offer.requirements.first().condition.children[1].fields[2].choices[0],QString("Not petrified"));
            QVERIFY(offer.requirements.last().title.contains("202"));QCOMPARE(offer.actions.first().scene,69);
            bool namedAnswer=false;
            for(const auto &a:offer.actions)for(const auto &f:a.condition.fields)if(f.kind=="dialogue-answer"){
                QVERIFY(a.title.startsWith("Dialogue answer"));QVERIFY(f.choices.contains(f.value));
                QVERIFY(!f.choices[f.value].startsWith("Answer ID"));QCOMPARE(a.condition.fields.size(),2);namedAnswer=true;
            }
            QVERIFY(namedAnswer);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void arrivalAndInvestigationRoutes(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();const auto baseline=project.manifest();
            const auto arrival=entry(EventFlow::read(project.rom(),*project.sceneEvents(),*project.scenePrograms(),0),0xec);
            QCOMPARE(arrival.trigger,QString("Arrive at map location"));QCOMPARE(arrival.actions.size(),4);
            const auto &condition=arrival.actions[0];QCOMPARE(condition.condition.fields.first().value,102);
            QVERIFY(condition.title.endsWith("is OFF"));QCOMPARE(condition.target,0xf4);QCOMPARE(condition.otherwise,0xf2);
            QCOMPARE(arrival.actions[1].target,0xfa);QCOMPARE(arrival.actions[2].fields.first().value,4);
            QVERIFY(arrival.actions[2].title.startsWith("Load event program"));
            QCOMPARE(arrival.actions[3].instructions,QVector<int>{0xfa});QCOMPARE(arrival.actions[3].title,QString("Stay on the world map"));
            const auto investigation=entry(EventFlow::read(project.rom(),*project.sceneEvents(),*project.scenePrograms(),70),0xa8);
            QCOMPARE(investigation.trigger,QString("Choose Area Investigation"));QCOMPARE(investigation.actions.first().fields.first().value,71);
            // Displaying the result and its branch destinations must not change
            // the stored event instructions or project data.
            QUndoStack undo;ScenarioEventEditor editor(&project,&undo,[]{});editor.showAllRows();editor.selectEventRow(0,0xec);
            bool worldMapOutcome=false;for(auto label:editor.findChildren<QLabel *>())worldMapOutcome|=label->text().contains("Stay on the world map");
            QVERIFY(worldMapOutcome);QCOMPARE(project.manifest(),baseline);
            QVERIFY(project.sceneEvents()->scenarioRows(1).contains(71));
            editor.selectEventRow(70,0xa8);
            auto open=editor.findChild<QPushButton *>("storyOpenProgram_172");QVERIFY(open&&open->isEnabled());open->click();
            QCOMPARE(editor.findChild<QComboBox *>("scenarioEventRow")->currentData().toInt(),71);
            QVERIFY(editor.findChild<QListWidget *>("storyEventList")->count()>1);QCOMPARE(project.manifest(),baseline);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void defeatedSquadConditionsAndEdits(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();
            auto flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),16);auto offer=entry(flow,330);
            auto branch=[&](const StoryEvent &event,int at)->const StoryAction&{for(const auto &a:event.actions)if(a.instructions.contains(at))return a;throw Error("Missing defeated-squad predicate");};
            QCOMPARE(offer.triggerFields.size(),1);QCOMPARE(offer.triggerFields.first().value,30);
            for(int at:{346,352,356,362,368,374,382}){const auto &a=branch(offer,at);QVERIFY(!a.advanced);QCOMPARE(a.kind,QString("condition"));QCOMPARE(a.condition.operation,QString("test"));QVERIFY(a.fields.isEmpty());}
            QVERIFY(branch(offer,346).title.contains("has no"));QCOMPARE(branch(offer,346).target,378);
            QCOMPARE(branch(offer,352).target,366);QVERIFY(branch(offer,356).title.contains("army roster"));QVERIFY(branch(offer,368).title.contains("opposing roster"));
            QVERIFY(branch(offer,362).title.contains("petrified"));QVERIFY(branch(offer,374).title.contains("petrified"));
            QCOMPARE(branch(offer,382).title,QString("Chaos Frame > 33"));
            auto findField=[&](const StoryAction &a,int at,const QString &key){for(const auto &f:a.condition.fields)if(f.offset==at&&f.key==key)return f;throw Error("Missing source field in predicate");};
            const auto squad=offer.triggerFields.first();QCOMPARE(squad.label,QString("Squad"));
            QCOMPARE(squad.choices[30],QString("Liedel (Archer) squad"));QCOMPARE(squad.choices[31],QString("Ninja Master squad"));
            QVERIFY(!branch(offer,352).title.contains("Liedel"));QVERIFY(findField(branch(offer,352),350,"value").kind!="scenario-squad");
            EventFlow::set(events,*project.scenePrograms(),16,squad,31);QCOMPARE(u8(events.program(16),333),quint8(31));
            EventFlow::set(events,*project.scenePrograms(),16,findField(branch(offer,346),340,"value"),1);
            QCOMPARE(u8(events.program(16),341),quint8(1));
            EventFlow::set(events,*project.scenePrograms(),16,findField(branch(offer,382),382,"comparison"),13);
            EventFlow::set(events,*project.scenePrograms(),16,findField(branch(offer,382),380,"value"),67);
            QCOMPARE(u16(events.program(16),382),quint16(0xd102));
            flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),16);offer=entry(flow,330);
            QCOMPARE(branch(offer,382).title,QString("Chaos Frame < 67"));QVERIFY(branch(offer,346).title.contains("Member 1"));
            project.setSceneEvents(events.json());QTemporaryDir work;project.saveFolder(work.filePath("project"));
            auto reopened=Project::fromRom(project.rom());reopened.applyManifest(Project::readProject(work.filePath("project")));reopened.loadSceneEvents();
            QCOMPARE(reopened.sceneEvents()->program(16),events.program(16));
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void authoredArrivalRuleProjection(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();
            const QJsonArray conditions{QJsonObject{{"flag",102},{"value",false}},QJsonObject{{"flag",3},{"value",true}}};
            events.setWorldRules(6,QJsonArray{QJsonObject{{"conditions",conditions},{"event",71},{"clearFrom",200},{"clearTo",255}}});
            const int appended=u16(events.program(0),12);auto flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),0);auto arrival=entry(flow,appended);
            QCOMPARE(arrival.actions.size(),3);QCOMPARE(arrival.actions[0].condition.operation,QString("all"));QCOMPARE(arrival.actions[0].condition.children.size(),2);
            QCOMPARE(arrival.actions[0].target,appended+12);QCOMPARE(arrival.actions[0].otherwise,appended+18);QCOMPARE(arrival.actions[2].target,0xec);
            auto state=arrival.actions[0].condition.children[0].fields[1];QCOMPARE(state.mode,StoryField::WorldRule);
            EventFlow::set(events,*project.scenePrograms(),0,state,1);
            QCOMPARE(u8(events.program(0),appended+4),quint8(0xac));
            EventFlow::set(events,*project.scenePrograms(),0,arrival.actions[1].fields[0],4);
            flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),0);arrival=entry(flow,appended);
            QVERIFY(arrival.actions[0].condition.children[0].title.endsWith("is ON"));QCOMPARE(arrival.actions[1].fields[0].value,4);
            project.setSceneEvents(events.json());QTemporaryDir work;project.saveFolder(work.filePath("project"));
            auto reopened=Project::fromRom(project.rom());reopened.applyManifest(Project::readProject(work.filePath("project")));reopened.loadSceneEvents();
            QCOMPARE(reopened.sceneEvents()->program(0),events.program(0));
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void defaultViewAndUndo(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();QUndoStack undo;
            ScenarioEventEditor editor(&project,&undo,[]{});editor.resize(1450,950);editor.showAllRows();editor.show();editor.selectEventRow(4,0xa8);QApplication::processEvents();
            auto raw=editor.findChild<QCheckBox *>("scenarioEventRaw");QVERIFY(raw);QVERIFY(!raw->isChecked());
            auto list=editor.findChild<QListWidget *>("storyEventList");QVERIFY(list&&list->isVisible());QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(),0xa8);
            auto visibleCombo=[&](const QString &name){for(auto c:editor.findChildren<QComboBox *>(name))if(c->isVisible())return c;return static_cast<QComboBox *>(nullptr);};
            auto town=visibleCombo("storyValue_170_value");QVERIFY(town);QCOMPARE(town->currentData().toInt(),2);
            const int choice=town->findData(3);town->setCurrentIndex(choice);QMetaObject::invokeMethod(town,"activated",Q_ARG(int,choice));
            QTRY_COMPARE(u8(project.sceneEvents()->program(4),0xab),quint8(3));
            undo.undo();QApplication::processEvents();QCOMPARE(u8(project.sceneEvents()->program(4),0xab),quint8(2));
            undo.redo();QApplication::processEvents();QCOMPARE(u8(project.sceneEvents()->program(4),0xab),quint8(3));
            editor.setRawInstructions(true);editor.selectEventRow(4,0xaa);QApplication::processEvents();
            auto scalar=editor.findChild<QSpinBox *>("scenarioEventParameter_value");QVERIFY(scalar&&scalar->isVisible());scalar->setValue(2);
            QTRY_COMPARE(u8(project.sceneEvents()->program(4),0xab),quint8(2));editor.setRawInstructions(false);QApplication::processEvents();
            town=visibleCombo("storyValue_170_value");QVERIFY(town);QCOMPARE(town->currentData().toInt(),2);
            QVERIFY(editor.grab().save("native/build/story-event-mulsuk.png"));
            editor.selectEventRow(10,74);QApplication::processEvents();QVERIFY(editor.grab().save("native/build/story-event-gunther.png"));
            editor.selectEventRow(67,14);QApplication::processEvents();
            QPushButton *next=nullptr;for(auto p:editor.findChildren<QPushButton *>("storyNextSteps"))if(p->isVisible())next=p;
            if(next){QVERIFY(next->isEnabled());next->click();QApplication::processEvents();}
            QVERIFY(editor.findChild<QScrollArea *>("storyEventDetails")->widget()->findChildren<QGroupBox *>().size()<=200);
            editor.selectEventRow(4,0xa8);QApplication::processEvents();
            for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole).toInt()==0xba)list->setCurrentRow(n);
            editor.setRawInstructions(true);QApplication::processEvents();
            QCOMPARE(editor.findChild<QTreeWidget *>("scenarioEventSteps")->currentItem()->data(0,Qt::UserRole).toInt(),0xba);
            QTemporaryDir folder;project.saveFolder(folder.path());auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(Project::readProject(folder.path()));reopened.loadSceneEvents();
            QCOMPARE(reopened.sceneEvents()->program(4),project.sceneEvents()->program(4));
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void exportReopen(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and N64 tools required");
        try{const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadSceneEvents();auto events=*project.sceneEvents();
            auto flow=EventFlow::read(rom,events,*project.scenePrograms(),4);const auto visit=entry(flow,0xa8);
            EventFlow::set(events,*project.scenePrograms(),4,visit.triggerFields[0],3);EventFlow::set(events,*project.scenePrograms(),4,visit.actions[0].fields[0],611);project.setSceneEvents(events.json());
            QTemporaryDir work;project.saveFolder(work.filePath("project"));auto loaded=Project::fromRom(rom);loaded.applyManifest(Project::readProject(work.filePath("project")));
            const auto built=rebuild(loaded,tools,work.path());auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);
            const auto output=Rom::fromBytes(built.image);auto reopened=Project::fromRom(output);reopened.loadSceneEvents();
            QCOMPARE(SceneEvents::readRow(output,4),events.program(4));QCOMPARE(reopened.sceneEvents()->json(),events.json());
            flow=EventFlow::read(output,*reopened.sceneEvents(),*reopened.scenePrograms(),4,false);QVERIFY(entry(flow,0xa8).simpleVisit);QCOMPARE(entry(flow,0xa8).actions.first().scene,611);
            QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);QCOMPARE(SceneEvents::retailRow(output,4),SceneEvents::retailRow(rom,4));
            QDir parent(QFileInfo(path).absolutePath());QVERIFY(parent.mkpath("Test ROMs"));QTemporaryDir folder(parent.filePath("Test ROMs/event-flow-XXXXXX"));QVERIFY(folder.isValid());folder.setAutoRemove(false);
            exportRom(loaded,built,folder.filePath(QFileInfo(path).fileName()));QFile event(folder.filePath("event-4.bin"));QVERIFY(event.open(QIODevice::WriteOnly));event.write(SceneEvents::readRow(output,4));event.close();
            qInfo().noquote()<<"Grouped event export:"<<folder.path();
        }catch(const std::exception &e){QFAIL(e.what());}
    }
};
QTEST_MAIN(EventFlowTests)
#include "event_flow_tests.moc"
