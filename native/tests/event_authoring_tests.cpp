#include "app/event_flow_editor.h"
#include "app/event_tree_dialog.h"
#include "app/campaign_editor.h"
#include "app/scenario_event_editor.h"
#include "app/theme.h"
#include "engine/event_authoring.h"
#include "engine/event_flow.h"
#include "engine/event_script.h"
#include "engine/entry_conditions.h"
#include "rebuild/compiler.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QInputDialog>
#include <QListWidget>
#include <QTableWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QTreeWidget>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QTimer>
#include <QFontDatabase>
#include <QUndoStack>
#include <QtTest>
using namespace lordly;
class EventAuthoringTests:public QObject {
 Q_OBJECT
 static QJsonObject tree(){
    const QJsonObject flag{{"type","flag"},{"flag",200},{"value",1}};
    const QJsonObject alternate{{"type","any"},{"conditions",QJsonArray{
        QJsonObject{{"type","chaos"},{"comparison",14},{"value",50}},QJsonObject{{"type","flag"},{"flag",201},{"value",1}}}}};
    const QJsonObject condition{{"type","all"},{"conditions",QJsonArray{flag,alternate}}};
    const QJsonObject branch{{"type","if"},{"condition",condition},
       {"then",QJsonArray{QJsonObject{{"type","flag"},{"flag",202},{"value",1}}}},
       {"else",QJsonArray{QJsonObject{{"type","flag"},{"flag",203},{"value",1}}}}};
    return {{"id","test-tree"},{"name","Encounter test"},{"group",3},{"target",30},{"nodes",QJsonArray{branch}}};
 }
 private slots:
 void initTestCase(){QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");qApp->setFont(QFont("Segoe UI",10));applyEditorTheme(*qApp);}
 void triggerAndState(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("ROM required");
   try{auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();const auto before=events.program(4);
     auto flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),4);QVector<StoryField> fields;
     for(const auto &e:flow)if(e.entry==0x102)for(const auto &a:e.actions)for(const auto &f:a.condition.fields)if(f.mode==StoryField::SquadState)fields<<f;
     QCOMPARE(fields.size(),2);QCOMPARE(fields[0].value,1);QCOMPARE(fields[1].value,0);
     EventFlow::set(events,*project.scenePrograms(),4,fields[1],1);QCOMPARE(u8(events.program(4),0x10b),129);QCOMPARE(u8(events.program(4),0x107),129);
     events.setTrigger(4,0x1bc,{{"group",2},{"guard",0x1bc},{"target",2}},*project.scenePrograms());const auto bytes=events.program(4);
     QCOMPARE(u8(bytes,0x1bd),0xf8);QCOMPARE(u8(bytes,0x1bf),2);
     auto contains=[&](int group){for(int at=u16(bytes,group*2);u16(bytes,at)!=65535;at+=2)if(u16(bytes,at)==0x1bc)return true;return false;};QVERIFY(contains(2));QVERIFY(!contains(3));
     for(int at=12;at<before.size();++at)if(at!=0x10b&&at!=0x1bd&&at!=0x1bf)QCOMPARE(bytes[at],before[at]);
     flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),4);for(const auto &e:flow)if(e.entry==0x1bc){QCOMPARE(e.triggerFields.size(),1);EventFlow::set(events,*project.scenePrograms(),4,e.triggerFields.first(),3);QCOMPARE(u8(events.program(4),0x1bf),3);EventFlow::set(events,*project.scenePrograms(),4,e.triggerFields.first(),2);}
     project.setSceneEvents(events.json());QTemporaryDir folder;project.saveFolder(folder.path());auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(Project::readProject(folder.path()));reopened.loadSceneEvents();QCOMPARE(reopened.sceneEvents()->program(4),bytes);
   }catch(const std::exception &e){QFAIL(e.what());}}
 void compilerBounds(){QByteArray source=QByteArray::fromHex("000c000c000c000c000c000cffff");auto t=tree();const auto bytes=EventAuthoring::compile(source,4,{t},{});QVERIFY(bytes.startsWith(source.left(6)));QVERIFY(bytes.size()>source.size());
   auto invalid=t;invalid["nodes"]=QJsonArray{QJsonObject{{"type","unknown"}}};QVERIFY_EXCEPTION_THROWN(EventAuthoring::compile(source,4,{invalid},{}),Error);
   invalid=t;invalid["target"]=300;QVERIFY_EXCEPTION_THROWN(EventAuthoring::compile(source,4,{invalid},{}),Error);
   QVERIFY_EXCEPTION_THROWN(EventAuthoring::compile(source,4,{t,t},{}),Error);
   auto condition=QJsonObject{{"type","flag"},{"flag",200},{"value",1}};for(int n=0;n<20;++n)condition={{"type","not"},{"condition",condition}};
   invalid=t;invalid["nodes"]=QJsonArray{QJsonObject{{"type","if"},{"condition",condition},{"then",QJsonArray{}},{"else",QJsonArray{}}}};QVERIFY_EXCEPTION_THROWN(EventAuthoring::compile(source,4,{invalid},{}),Error);
 }
 void creationAndUndo(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("ROM required");
   try{auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();const auto original=project.sceneEvents()->json();QUndoStack undo;CampaignEditor campaign(&project,&undo,[]{});campaign.selectEvent(10);QWidget *editor=nullptr;
     auto pane=campaign.findChild<QWidget *>("storylineEvents");QVERIFY(pane);QCOMPARE(pane->findChild<QComboBox *>("storylineEventRow")->currentData().toInt(),10);editor=pane->findChild<QWidget *>("storyEventsView");QVERIFY(editor);
     QTimer::singleShot(0,[] {auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->findChild<QLineEdit *>("eventTreeName")->setText("New branching event");dialog->findChild<QPushButton *>("eventTreeAddIf")->click();QCOMPARE(dialog->findChild<QTreeWidget *>("eventTreeSteps")->topLevelItemCount(),1);QApplication::processEvents();dialog->grab().save("native/build/event-tree-authoring.png");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();if(dialog->isVisible()){for(auto label:dialog->findChildren<QLabel *>())qInfo()<<label->text();dialog->reject();}});
     editor->findChild<QPushButton *>("storyCreateEvent")->click();QCOMPARE(undo.count(),1);QCOMPARE(project.sceneEvents()->flagEdits()["10"].toObject()["trees"].toArray().size(),1);
     QTRY_VERIFY(editor->findChild<QListWidget *>("storyEventList")->currentItem()->text().contains("New branching event"));
     const auto authored=project.sceneEvents()->json();undo.undo();QCOMPARE(project.sceneEvents()->json(),original);undo.redo();QCOMPARE(project.sceneEvents()->json(),authored);
     QTRY_VERIFY(editor->findChild<QListWidget *>("storyEventList")->currentItem()->text().contains("New branching event"));
   }catch(const std::exception &e){QFAIL(e.what());}}
 void customNamesAndCancel(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("ROM required");
   try{auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();const int id=events.createFlag("Cayes helped");QCOMPARE(id,256);events.renameFlag(id,"Cayes rescued");QVERIFY(events.flagChoices(10)[id].contains("Cayes rescued"));
     QVERIFY_EXCEPTION_THROWN(events.createFlag("cayes rescued"),Error);QVERIFY_EXCEPTION_THROWN(events.createFlag(" "),Error);
     auto t=tree();t["nodes"]=QJsonArray{QJsonObject{{"type","flag"},{"flag",id},{"value",1}}};events.setTrees(10,{t},*project.scenePrograms());QVERIFY_EXCEPTION_THROWN(events.removeFlag(id),Error);
     events.setTrees(10,{},*project.scenePrograms());events.removeFlag(id);QVERIFY(!events.flagChoices().contains(id));QCOMPARE(events.createFlag("Cayes again"),257);
     const auto original=project.sceneEvents()->json();QUndoStack undo;EventFlowEditor editor(&project,&undo,[&]{editor.setRow(10);});editor.setRow(10);
     auto createInDialog=[](bool cancel){auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);QTimer::singleShot(15000,dialog,&QDialog::reject);dialog->findChild<QPushButton *>("eventTreeAddIf")->click();auto combo=dialog->findChild<QComboBox *>("eventTreeFlag");QVERIFY(combo);
       auto create=dialog->findChild<QPushButton *>("eventTreeCreateFlag");QVERIFY(create);QTRY_VERIFY(create->isVisible());
       QTimer::singleShot(0,[]{auto input=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(input);input->reject();});
       create->click();QCOMPARE(combo->currentData().toInt(),200);
       QTimer::singleShot(0,[]{auto input=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(input);input->setTextValue("Katreda joined my quest");input->accept();});
       create->click();QCOMPARE(combo->currentData().toInt(),256);QVERIFY(combo->currentText().contains("Katreda joined my quest"));
       QTimer::singleShot(0,[]{auto input=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(input);input->setTextValue(EventAuthoring::actions().value("flag"));input->accept();});
       dialog->findChild<QPushButton *>("eventTreeAddAction")->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
       combo=dialog->findChild<QComboBox *>("eventTreeFlag");QVERIFY(combo);QVERIFY(combo->findData(256)>=0);
       create=dialog->findChild<QPushButton *>("eventTreeCreateFlag");QVERIFY(create);QTRY_VERIFY(create->isVisible());
       QTimer::singleShot(0,[]{auto input=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(input);input->setTextValue("Quest reward received");input->accept();});
       create->click();QCOMPARE(combo->currentData().toInt(),257);QVERIFY(combo->currentText().contains("Quest reward received"));
       if(!cancel){QApplication::processEvents();QVERIFY(dialog->grab().save("native/build/event-tree-create-flag.png"));}
       dialog->findChild<QDialogButtonBox *>()->button(cancel?QDialogButtonBox::Cancel:QDialogButtonBox::Ok)->click();};
     QTimer::singleShot(0,[=]{createInDialog(true);});editor.findChild<QPushButton *>("storyCreateEvent")->click();QCOMPARE(project.sceneEvents()->json(),original);QCOMPARE(undo.count(),0);
     QTimer::singleShot(0,[=]{createInDialog(false);});editor.findChild<QPushButton *>("storyCreateEvent")->click();QCOMPARE(undo.count(),1);QVERIFY(project.sceneEvents()->flagName(256).contains("Katreda joined my quest"));QVERIFY(project.sceneEvents()->flagName(257).contains("Quest reward received"));
     const auto branch=project.sceneEvents()->flagEdits()["10"].toObject()["trees"].toArray().first().toObject()["nodes"].toArray().first().toObject();
     QCOMPARE(branch["condition"].toObject()["flag"].toInt(),256);QCOMPARE(branch["then"].toArray().first().toObject()["flag"].toInt(),257);
     const auto edited=project.sceneEvents()->json();undo.undo();QCOMPARE(project.sceneEvents()->json(),original);undo.redo();QCOMPARE(project.sceneEvents()->json(),edited);
     QTemporaryDir folder;project.saveFolder(folder.path());auto loaded=Project::fromRom(Rom::open(path));loaded.applyManifest(Project::readProject(folder.path()));loaded.loadSceneEvents();QCOMPARE(loaded.sceneEvents()->json(),edited);
     QVERIFY(EventScript::flagName(170,36).contains("Yumil"));QVERIFY(!EventScript::flagName(170,9).contains("Unknown"));
   }catch(const std::exception &e){QFAIL(e.what());}}
 void visitorRequirements(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("ROM required");
   try{auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();const auto original=events.program(10);
     auto guard=[&]{for(const auto &e:EventFlow::read(project.rom(),events,*project.scenePrograms(),10))if(e.entry==74)return e.requirements.first();throw Error("Missing Cayes guard");};
     const auto requirement=guard();QCOMPARE(requirement.condition.children.size(),2);const auto squad=requirement.condition.children[0].fields,member=requirement.condition.children[1].fields;
     QCOMPARE(squad.size(),2);QCOMPARE(member.size(),3);QCOMPARE(member[1].value,1);QCOMPARE(member[2].value,0);
     for(int state:{-1,0,1}){EventFlow::set(events,*project.scenePrograms(),10,member[1],state);EventFlow::set(events,*project.scenePrograms(),10,member[2],state);const auto next=guard();QCOMPARE(next.condition.children[1].fields[1].value,state);QCOMPARE(next.condition.children[1].fields[2].value,state);}
     EventFlow::set(events,*project.scenePrograms(),10,member[0],2);EventFlow::set(events,*project.scenePrograms(),10,squad[1],1);QCOMPARE(guard().condition.children[1].fields[0].value,2);
     project.setSceneEvents(events.json());QTemporaryDir folder;project.saveFolder(folder.path());auto loaded=Project::fromRom(Rom::open(path));loaded.applyManifest(Project::readProject(folder.path()));loaded.loadSceneEvents();QCOMPARE(loaded.sceneEvents()->program(10),events.program(10));
     EventFlow::set(events,*project.scenePrograms(),10,member[0],0);EventFlow::set(events,*project.scenePrograms(),10,member[1],1);EventFlow::set(events,*project.scenePrograms(),10,member[2],0);EventFlow::set(events,*project.scenePrograms(),10,squad[1],0);QCOMPARE(events.program(10),original);
   }catch(const std::exception &e){QFAIL(e.what());}}
 void customFlagExport(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");if(path.isEmpty()||tools.isEmpty())QSKIP("ROM and tools required");
   try{const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadSceneEvents();QTemporaryDir work;
     QCOMPARE(rebuild(project,tools,work.path()).image,rom.bytes());
     auto events=*project.sceneEvents();for(int i=0;i<512;++i)QCOMPARE(events.createFlag(QString("Quest flag %1").arg(i+1)),256+i);
     QVERIFY_EXCEPTION_THROWN(events.createFlag("Overflow"),Error);auto t=tree();t["nodes"]=QJsonArray{QJsonObject{{"type","if"},{"condition",QJsonObject{{"type","flag"},{"flag",256},{"value",1}}},
       {"then",QJsonArray{QJsonObject{{"type","flag"},{"flag",767},{"value",1}}}},{"else",QJsonArray{QJsonObject{{"type","flag"},{"flag",767},{"value",0}}}}}};
     events.setTrees(4,{t},*project.scenePrograms());
     const auto world=events.program(0);const int entry=u16(world,40);bool entryChanged=false;
     for(int at=entry;at+2<world.size();at+=2)if(u16(world,at)==0x6980){events.editParameter(0,at,"value",129,*project.scenePrograms());entryChanged=true;break;}QVERIFY(entryChanged);
     for(const auto &e:EventFlow::read(rom,events,*project.scenePrograms(),10))if(e.entry==74){const auto member=e.requirements.first().condition.children[1].fields;EventFlow::set(events,*project.scenePrograms(),10,member[0],2);EventFlow::set(events,*project.scenePrograms(),10,member[1],0);EventFlow::set(events,*project.scenePrograms(),10,member[2],-1);}
     project.setSceneEvents(events.json());const auto built=rebuild(project,tools,work.path());const auto out=Rom::fromBytes(built.image);auto reopened=Project::fromRom(out);reopened.loadSceneEvents();
     QCOMPARE(reopened.sceneEvents()->json(),events.json());QCOMPARE(SceneEvents::readRow(out,0),events.program(0));QCOMPARE(SceneEvents::readRow(out,4),events.program(4));QCOMPARE(SceneEvents::readRow(out,10),events.program(10));QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
     auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);
     QDir parent(QFileInfo(path).absolutePath());parent.mkpath("Test ROMs");QTemporaryDir folder(parent.filePath("Test ROMs/custom-flags-XXXXXX"));folder.setAutoRemove(false);exportRom(project,built,folder.filePath(QFileInfo(path).fileName()));
     QFile script(folder.filePath("event-4.bin"));QVERIFY(script.open(QIODevice::WriteOnly));script.write(SceneEvents::readRow(out,4));script.close();qInfo().noquote()<<"Custom flag export:"<<folder.path();
     QFile visitor(folder.filePath("event-10.bin"));QVERIFY(visitor.open(QIODevice::WriteOnly));visitor.write(SceneEvents::readRow(out,10));visitor.close();
     QFile arrival(folder.filePath("event-0.bin"));QVERIFY(arrival.open(QIODevice::WriteOnly));arrival.write(SceneEvents::readRow(out,0));arrival.close();
   }catch(const std::exception &e){QFAIL(e.what());}}
 void entryConditions(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("ROM required");
   try{auto project=Project::fromRom(Rom::open(path));QUndoStack undo;CampaignEditor campaign(&project,&undo,[]{});
     auto locations=campaign.findChild<QComboBox *>("storylineNode");locations->setCurrentIndex(locations->findData(20));auto table=campaign.findChild<QTableWidget *>("storylineRules");QCOMPARE(table->rowCount(),3);table->setCurrentCell(1,0);
     auto panel=campaign.findChild<QScrollArea *>("storylineBranchDetails");QVERIFY(panel);auto state=panel->widget()->findChild<QComboBox *>("branchFlagState_30");QVERIFY(state);QCOMPARE(state->currentData().toInt(),1);
     const auto before=project.sceneEvents()->program(0);state->setCurrentIndex(0);QMetaObject::invokeMethod(state,"activated",Q_ARG(int,0));QTRY_COMPARE(undo.count(),1);QVERIFY(project.sceneEvents()->program(0)!=before);
     QCOMPARE(table->item(table->currentRow(),1)->data(Qt::UserRole).toInt(),30);QCOMPARE(panel->widget()->findChild<QComboBox *>("branchFlagState_30")->currentData().toInt(),0);
     undo.undo();QCOMPARE(project.sceneEvents()->program(0),before);undo.redo();QVERIFY(project.sceneEvents()->program(0)!=before);undo.undo();
     table->setCurrentCell(0,0);auto flag=panel->widget()->findChild<QComboBox *>("branchFlag_128");QVERIFY(flag);const int choice=flag->findData(129);flag->setCurrentIndex(choice);QMetaObject::invokeMethod(flag,"activated",Q_ARG(int,choice));QTRY_COMPARE(undo.index(),1);
     bool found=false;for(int i=0;i<table->rowCount();++i)found|=table->item(i,0)->text().contains("Flag 129");QVERIFY(found);undo.undo();QCOMPARE(project.sceneEvents()->program(0),before);
     campaign.findChild<QTabWidget *>("campaignTabs")->setCurrentIndex(2);campaign.findChild<QTabWidget *>("storylineTabs")->setCurrentIndex(1);campaign.resize(1600,900);campaign.show();QApplication::processEvents();QVERIFY(campaign.grab().save("native/build/branch-direct-conditions.png"));
   }catch(const std::exception &e){QFAIL(e.what());}}
 void followFlagWriter(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("ROM required");
   try{auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();QUndoStack undo;ScenarioEventEditor editor(&project,&undo,[]{});editor.showAllRows();editor.selectEventRow(30);editor.resize(1450,950);editor.show();QApplication::processEvents();
     int check=-1;for(const auto &event:EventFlow::read(project.rom(),*project.sceneEvents(),*project.scenePrograms(),30))for(const auto &action:event.actions)for(const auto &f:action.condition.fields)if(f.label=="Flag"&&f.value==200){check=action.instructions.first();break;}QVERIFY(check>=0);editor.selectEventRow(30,check);
     auto link=editor.findChild<QScrollArea *>("storyEventDetails")->widget()->findChild<QLabel *>("flagSourcesOn_200");QVERIFY(link);
     QTimer::singleShot(0,[]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);auto list=dialog->findChild<QListWidget *>("flagWriterChoices");QVERIFY(list);QCOMPARE(list->count(),2);list->setCurrentRow(1);dialog->accept();});
     QMetaObject::invokeMethod(link,"linkActivated",Q_ARG(QString,QString("open")));QCOMPARE(editor.findChild<QComboBox *>("scenarioEventRow")->currentData().toInt(),30);
     auto flow=dynamic_cast<EventFlowEditor *>(editor.findChild<QWidget *>("storyEventsView"));QVERIFY(flow);QVERIFY(flow->selectedOffset()==0x1cc||flow->selectedOffset()==0x1f4);QCOMPARE(undo.count(),0);
     QApplication::processEvents();QVERIFY(editor.grab().save("native/build/flag-writer-destination.png"));
   }catch(const std::exception &e){QFAIL(e.what());}}
 void branchExportReopen(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");if(path.isEmpty()||tools.isEmpty())QSKIP("ROM and tools required");
   try{const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadSceneEvents();auto events=*project.sceneEvents();const auto &scenes=*project.scenePrograms();QTemporaryDir work;
     QCOMPARE(rebuild(project,tools,work.path()).image,rom.bytes());
     auto entryFor=[&](int node,int chaos,int event){StoryRoutes routes(rom);routes.scripts[0]=events.program(0);for(const auto &route:routes.routes(node,chaos))if(route.event==event)return EntryConditions::read(rom,events,scenes,route,chaos);throw Error("Missing branch export fixture");};
     auto entry=entryFor(20,50,30);bool changed=false;for(const auto &flag:entry.flags)if(flag.flag==30){EntryConditions::setState(events,scenes,flag,0);changed=true;break;}QVERIFY(changed);
     entry=entryFor(20,50,30);changed=false;for(const auto &flag:entry.flags)if(flag.flag==30){EntryConditions::setFlag(events,scenes,flag,29);changed=true;break;}QVERIFY(changed);
     EntryConditions::setEvent(events,scenes,entryFor(20,50,30),32);
     StoryRoutes routes(rom);routes.scripts[0]=events.program(0);int chaosNode=-1,chaosEvent=-1;QMap<int,int> chaosFlags;
     for(int node:routes.nodes.keys()){for(const auto &route:routes.routes(node,33)){const auto e=EntryConditions::read(rom,events,scenes,route,33);if(e.numbers.isEmpty())continue;chaosNode=node;chaosEvent=route.event;chaosFlags=route.requirements;EntryConditions::setNumberValue(events,scenes,e.numbers.first(),37);break;}if(chaosNode>=0)break;}QVERIFY(chaosNode>0);
     project.setSceneEvents(events.json());project.saveFolder(work.filePath("project"));auto loaded=Project::fromRom(rom);loaded.applyManifest(Project::readProject(work.filePath("project")));loaded.loadSceneEvents();QCOMPARE(loaded.sceneEvents()->program(0),events.program(0));
     const auto built=rebuild(loaded,tools,work.path());const auto output=Rom::fromBytes(built.image);auto reopened=Project::fromRom(output);reopened.loadSceneEvents();QCOMPARE(reopened.sceneEvents()->json(),events.json());QCOMPARE(SceneEvents::readRow(output,0),events.program(0));QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
     auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);
     QDir parent(QFileInfo(path).absolutePath());parent.mkpath("Test ROMs");QTemporaryDir folder(parent.filePath("Test ROMs/entry-conditions-XXXXXX"));folder.setAutoRemove(false);exportRom(loaded,built,folder.filePath(QFileInfo(path).fileName()));
     QFile script(folder.filePath("event-0.bin"));QVERIFY(script.open(QIODevice::WriteOnly));script.write(SceneEvents::readRow(output,0));script.close();
     QJsonObject flags;for(auto i=chaosFlags.begin();i!=chaosFlags.end();++i)flags[QString::number(i.key())]=i.value();QFile fixture(folder.filePath("entry-fixture.json"));QVERIFY(fixture.open(QIODevice::WriteOnly));fixture.write(QJsonDocument(QJsonObject{{"node",chaosNode},{"event",chaosEvent},{"flags",flags},{"threshold",37}}).toJson());fixture.close();qInfo().noquote()<<"Entry condition export:"<<folder.path();
   }catch(const std::exception &e){QFAIL(e.what());}}
 void exportReopen(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");if(path.isEmpty()||tools.isEmpty())QSKIP("ROM and tools required");
   try{const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadSceneEvents();auto events=*project.sceneEvents();
     QJsonArray longThen;for(int i=0;i<90;++i)longThen<<QJsonObject{{"type","flag"},{"flag",205},{"value",1}};
     auto longTree=tree();longTree["id"]="long-tree";longTree["name"]="Long branch test";longTree["nodes"]=QJsonArray{QJsonObject{{"type","if"},
         {"condition",QJsonObject{{"type","not"},{"condition",QJsonObject{{"type","flag"},{"flag",200},{"value",1}}}}},
         {"then",longThen},{"else",QJsonArray{QJsonObject{{"type","flag"},{"flag",206},{"value",1}}}}}};
     events.setTrees(4,{tree(),longTree},*project.scenePrograms());events.setTrigger(4,0x1bc,{{"group",2},{"guard",0x1bc},{"target",2}},*project.scenePrograms());project.setSceneEvents(events.json());
     QTemporaryDir work;project.saveFolder(work.filePath("project"));auto loaded=Project::fromRom(rom);loaded.applyManifest(Project::readProject(work.filePath("project")));const auto built=rebuild(loaded,tools,work.path());
     const auto out=Rom::fromBytes(built.image);auto reopened=Project::fromRom(out);reopened.loadSceneEvents();QCOMPARE(reopened.sceneEvents()->json(),events.json());QCOMPARE(SceneEvents::readRow(out,4),events.program(4));QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
     auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);QCOMPARE(SceneEvents::retailRow(out,4),SceneEvents::retailRow(rom,4));
     QDir parent(QFileInfo(path).absolutePath());parent.mkpath("Test ROMs");QTemporaryDir folder(parent.filePath("Test ROMs/event-authoring-XXXXXX"));folder.setAutoRemove(false);exportRom(loaded,built,folder.filePath(QFileInfo(path).fileName()));
     QFile script(folder.filePath("event-4.bin"));QVERIFY(script.open(QIODevice::WriteOnly));script.write(SceneEvents::readRow(out,4));script.close();qInfo().noquote()<<"Authored export:"<<folder.path();
     auto edited=*reopened.sceneEvents();edited.setTrees(4,{},*reopened.scenePrograms());edited.setTrigger(4,0x1bc,{},*reopened.scenePrograms());QCOMPARE(edited.program(4),SceneEvents::retailRow(rom,4));
   }catch(const std::exception &e){QFAIL(e.what());}}
};
QTEST_MAIN(EventAuthoringTests)
#include "event_authoring_tests.moc"
