#include "app/event_flow_editor.h"
#include "app/event_tree_dialog.h"
#include "app/campaign_editor.h"
#include "app/scenario_event_editor.h"
#include "app/scenario_flowchart_model.h"
#include "app/theme.h"
#include "engine/event_authoring.h"
#include "engine/event_flag_links.h"
#include "engine/new_campaign.h"
#include "engine/squad_members.h"
#include "engine/capacity_placement.h"
#include "engine/mission_runtime.h"
#include "engine/mission_tables.h"
#include "engine/event_flow.h"
#include "engine/event_script.h"
#include "engine/entry_conditions.h"
#include "rebuild/compiler.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLineF>
#include <QComboBox>
#include <QCheckBox>
#include <QInputDialog>
#include <QMessageBox>
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
 void newCampaignModel(){
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
   try {
     auto p=Project::fromRom(Rom::open(path));p.loadCampaignMap();p.loadWorld();p.loadScenarios();p.loadSceneEvents();
     const auto before=p.manifest(),world=p.world()->json(),map=p.campaignMap()->json();
     auto config=NewCampaign::defaults(p,6);NewCampaign::initialize(p,config);
     QCOMPARE(p.world()->json(),world);QCOMPARE(p.campaignMap()->json(),map);
     for(const auto &s:p.scenarios()->entries()){QVERIFY(s.squads.isEmpty());QVERIFY(s.layout.deployments.isEmpty());QVERIFY(s.layout.conditions.isEmpty());}
     StoryRoutes routes(p.rom());routes.scripts[0]=p.sceneEvents()->program(0);
     for(int node:routes.nodes.keys())QVERIFY(routes.routes(node).isEmpty());
     for(int node=1;node<=53;++node)QCOMPARE(u16(routes.scripts[0],node*2),0);
     for(int node=1;node<=52;++node)QCOMPARE(u16(p.sceneEvents()->program(70),node*2),0);
     QVERIFY(p.sceneEvents()->program(1).contains(QByteArray::fromHex("0901")));
     auto army=SaveDocument::open(p.runtimeRom(),NewCampaign::armyBytes(config,0));QCOMPARE(army.units().size(),1);QVERIFY(u16(army.memory(),0x193bc0+56+24)>0);
     auto armies=config["armies"].toArray();armies.append(NewCampaign::army(p,1,"Second army",army.serialize()));config["armies"]=armies;NewCampaign::configure(p,config);
     config["army"]=1;NewCampaign::configure(p,config);
     QCOMPARE(p.sceneEvents()->flagEdits()["1"].toObject()["trees"].toArray()[0].toObject()["nodes"].toArray()[1].toObject()["army"].toInt(),1);
     config["army"]=0;NewCampaign::configure(p,config);
     auto incomplete=*p.sceneEvents();auto broken=incomplete.flagEdits()["1"].toObject()["trees"].toArray();auto brokenTree=broken[0].toObject();
     auto brokenNodes=brokenTree["nodes"].toArray();brokenNodes.removeLast();brokenTree["nodes"]=brokenNodes;broken[0]=brokenTree;
     QVERIFY_EXCEPTION_THROWN(incomplete.setTrees(1,broken,*p.scenePrograms()),Error);
     auto events=*p.sceneEvents();auto trees=events.flagEdits()["1"].toObject()["trees"].toArray();auto opening=trees[0].toObject();auto nodes=opening["nodes"].toArray();
     nodes.insert(2,QJsonObject{{"type","if"},{"condition",QJsonObject{{"type","answer"},{"value",1}}},{"then",QJsonArray{QJsonObject{{"type","startingArmy"},{"army",1}}}},{"else",QJsonArray{}}});
     opening["nodes"]=nodes;trees[0]=opening;events.setTrees(1,trees,*p.scenePrograms());p.setSceneEvents(events.json());
     auto reopened=Project::fromRom(p.rom());reopened.applyManifest(p.manifest());QCOMPARE(reopened.sceneEvents()->json(),p.sceneEvents()->json());
     auto squads=*reopened.scenarios();const int added=squads.addSquad(1,-1,reopened.classData());squads.removeSquad(1,added,reopened.classData());QVERIFY(squads.scenario(1).squads.isEmpty());
     auto missing=config;auto only=missing["armies"].toArray();only.removeAt(1);missing["armies"]=only;
     QVERIFY_EXCEPTION_THROWN(NewCampaign::configure(reopened,missing),Error);
     QCOMPARE(reopened.sceneEvents()->json(),p.sceneEvents()->json());
     reopened.applyManifest(before);QCOMPARE(reopened.sceneEvents()->program(1),SceneEvents::retailRow(p.rom(),1));
   }catch(const std::exception &e){QFAIL(e.what());}
 }
 void newCampaignLayoutOptions_data(){
   QTest::addColumn<bool>("keepWorld");QTest::addColumn<bool>("keepMaps");
   QTest::newRow("keep-both")<<true<<true;QTest::newRow("keep-world")<<true<<false;
   QTest::newRow("keep-maps")<<false<<true;QTest::newRow("empty")<<false<<false;
 }
 void newCampaignLayoutOptions(){
   QFETCH(bool,keepWorld);QFETCH(bool,keepMaps);
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
   try {
     auto p=Project::fromRom(Rom::open(path));p.loadWorld();p.loadCampaignMap();p.loadScenarios();
     const auto world=p.world()->json(),map=p.campaignMap()->json();
     auto config=NewCampaign::defaults(p,6);config["keepWorldMap"]=keepWorld;config["keepScenarioMaps"]=keepMaps;NewCampaign::initialize(p,config);
     QCOMPARE(p.world()->json(),world);
     if(keepWorld)QCOMPARE(p.campaignMap()->json(),map);
     else {
       for(int id=1;id<=40;++id)QCOMPARE(p.campaignMap()->position(id),id==6?QPoint(256,160):QPoint());
       QCOMPARE(p.campaignMap()->availableNodes(true).size(),39);
       for(int id=1;id<=p.campaignMap()->totalRoads();++id)QCOMPARE(p.campaignMap()->road(id),qMakePair(0,0));
     }
     for(int key:p.scenarios()->entries().keys())QCOMPARE(NewCampaign::includesBattlefield(*p.sceneEvents(),key),keepMaps);
     auto reopened=Project::fromRom(p.runtimeRom());reopened.applyManifest(p.manifest());reopened.loadCampaignMap();
     QCOMPARE(reopened.campaignMap()->json(),p.campaignMap()->json());QCOMPARE(reopened.sceneEvents()->json(),p.sceneEvents()->json());
   }catch(const std::exception &e){QFAIL(e.what());}
 }
 void newCampaignDialogUndo_data(){QTest::addColumn<bool>("keep");QTest::newRow("keep")<<true;QTest::newRow("empty")<<false;}
 void newCampaignDialogUndo(){
   QFETCH(bool,keep);
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
   try {
     auto p=Project::fromRom(Rom::open(path));QUndoStack undo;CampaignEditor editor(&p,&undo,[]{});const auto before=p.manifest();
     auto create=editor.findChild<QPushButton *>("campaignNew");QVERIFY(create);
     bool dialogSeen=false;QTimer timer;QObject::connect(&timer,&QTimer::timeout,&editor,[&]{
       auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog||dialog->objectName()!="campaignSetupDialog")return;
       timer.stop();dialogSeen=true;auto start=dialog->findChild<QComboBox *>("campaignStartLocation");QVERIFY(start);start->setCurrentIndex(start->findData(7));
       for(auto id:{"campaignKeepWorldMap","campaignKeepScenarioMaps"}){auto box=dialog->findChild<QCheckBox *>(id);QVERIFY(box);QVERIFY(box->isChecked());box->setChecked(keep);}
       auto armies=dialog->findChild<QListWidget *>("campaignStartingArmies");QVERIFY(armies);QCOMPARE(armies->count(),1);
       auto buttons=dialog->findChild<QDialogButtonBox *>();QVERIFY(buttons);buttons->button(QDialogButtonBox::Ok)->click();
       if(dialog->isVisible())dialog->reject();
     });timer.start(20);create->click();timer.stop();QVERIFY(dialogSeen);QCOMPARE(undo.count(),1);
     QCOMPARE(NewCampaign::settings(*p.sceneEvents())["location"].toInt(),7);const auto after=p.manifest();
     QCOMPARE(NewCampaign::settings(*p.sceneEvents())["keepWorldMap"].toBool(),keep);
     QCOMPARE(NewCampaign::settings(*p.sceneEvents())["keepScenarioMaps"].toBool(),keep);
     if(!keep)QCOMPARE(editor.findChild<QListWidget *>("scenarioList")->count(),0);
     undo.undo();QCOMPARE(p.manifest(),before);undo.redo();QCOMPARE(p.manifest(),after);
   }catch(const std::exception &e){QFAIL(e.what());}
 }
 void newCampaignExport_data(){QTest::addColumn<bool>("expanded");QTest::addColumn<bool>("empty");QTest::newRow("ordinary")<<false<<false;QTest::newRow("expanded")<<true<<false;QTest::newRow("empty")<<false<<true;}
 void newCampaignExport(){
   QFETCH(bool,expanded);QFETCH(bool,empty);
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"),out=qEnvironmentVariable("LORDLY_CAMPAIGN_OUTPUT");
   if(path.isEmpty()||tools.isEmpty()||out.isEmpty())QSKIP("Private ROM, toolchain and owned output required");
   try {
     auto p=Project::fromRom(Rom::open(path));auto config=NewCampaign::defaults(p,6);
     auto army=SaveDocument::open(p.runtimeRom(),NewCampaign::armyBytes(config,0));
     army.apply(army.planEdit(SaveTable::Characters,1,"name","Choice Two"),true);
     auto armies=config["armies"].toArray();armies.append(NewCampaign::army(p,1,"Second army",army.serialize()));config["armies"]=armies;
     config["goth"]=1234;config["chaos"]=37;config["locations"]=QJsonArray{6,7};config["keepWorldMap"]=!empty;config["keepScenarioMaps"]=!empty;NewCampaign::initialize(p,config);
     const auto originalSquads=Scenarios::load(p.rom()).scenario(1).squads;
     const int key=p.availableScenarioBattlefields().first();p.createScenarioBattlefield(key,1,"exact","Campaign battle");
     QCOMPARE(p.scenarios()->scenario(key).squads,originalSquads);
     if(expanded){auto scenarios=*p.scenarios();QVector<CustomMember> members;for(int cell=0;cell<5;++cell)members.append({5,0,8-cell,{0,0}});
       for(int row=0;row<scenarios.scenario(key).squads.size();++row)scenarios.setSquad(key,row,editSquadMembers(scenarios.scenario(key).squads[row],members),p.classData());
       while(scenarios.scenario(key).squads.size()<30)scenarios.addSquad(key,0,p.classData());p.setScenarios(scenarios.json());QVERIFY(p.expandedArmies());}
     auto missionEvents=*p.sceneEvents();const int mission=missionEvents.createMission(6,key,"Campaign battle",-1,-1,*p.scenePrograms());p.setSceneEvents(missionEvents.json());
     p.loadEncounters();
     if(expanded){auto encounters=p.encounters()->json();auto rates=encounters["scenarioRates"].toObject();
       rates["rows"]=QJsonObject{{QString::number(key),QJsonObject{{"normal",QJsonObject{{"mode","override"},{"divisor",100},{"passCount",27}}},{"alternate",QJsonObject{{"mode","inherit"}}}}}};
       encounters["scenarioRates"]=rates;p.setEncounters(encounters);}
     auto events=*p.sceneEvents();auto trees=events.flagEdits()["1"].toObject()["trees"].toArray();auto tree=trees[0].toObject();auto nodes=tree["nodes"].toArray();
     nodes.insert(2,QJsonObject{{"type","if"},{"condition",QJsonObject{{"type","answer"},{"value",1}}},{"then",QJsonArray{QJsonObject{{"type","startingArmy"},{"army",1}}}},{"else",QJsonArray{}}});
     tree["nodes"]=nodes;trees[0]=tree;events.setTrees(1,trees,*p.scenePrograms());p.setSceneEvents(events.json());
     QTemporaryDir work;const auto result=rebuild(p,tools,work.path());auto rom=Rom::fromBytes(result.image);auto reopened=Project::fromRom(rom);reopened.loadSceneEvents();
     reopened.loadTexts();reopened.loadPromotion();reopened.loadShops();reopened.loadConsumableEffects();reopened.loadEncounters();
     QCOMPARE(reopened.encounters()->json(),p.encounters()->json());
     reopened.loadCampaignMap();for(int id=1;id<=40;++id)QCOMPARE(reopened.campaignMap()->position(id),p.campaignMap()->position(id));
     QCOMPARE(reopened.campaignMap()->roadBytes(),p.campaignMap()->roadBytes());QCOMPARE(reopened.campaignMap()->backgroundBytes(),p.campaignMap()->backgroundBytes());
     QCOMPARE(reopened.sceneEvents()->json(),p.sceneEvents()->json());QCOMPARE(SceneEvents::readRow(rom,1),events.program(1));
     // The native world dispatcher calls the VM at tableBase + selectedOffset,
     // including when no event is assigned to the location.
     for(const auto [row,count]:{qMakePair(0,54),qMakePair(70,53)}) {
       const auto routing=SceneEvents::readRow(rom,row);
       QCOMPARE(routing,reopened.sceneEvents()->program(row));
       QCOMPARE(u16(routing,count*2),65535);
       for(int location=row==0?1:0;location<count;++location) {
         if(row==0&&location==6)continue; // Authored arrival branch.
         const int entry=u16(routing,location*2);
         QVERIFY(entry>=2*(count+1)&&entry+2<=routing.size());
         QCOMPARE(u16(routing,entry),0x2f00);
       }
     }
     const EventContexts contexts(rom,*reopened.sceneEvents());
     QVERIFY(contexts.location(6).phases[2].isEmpty());
     QVERIFY(contexts.location(7).phases[0].isEmpty());
     // Director 1078 uses the native directory key immediate and indexes
     // 32 + battlefield key, including the upper end of the custom range.
     constexpr int resourceBase=0x594280, retailTitleDirectory=0x1e24f10;
     const auto &image=rom.bytes();
     const auto titleDirectory=resourceBase+int((u32(image,0x289404)&65535)<<16 |
                                                (u32(image,0x289408)&65535));
     QVERIFY(titleDirectory>=0x2800000);
     QCOMPARE(u32(image,titleDirectory),quint32(4*(32+MissionTables::keyCount)));
     QCOMPARE(image.mid(titleDirectory+4,94*4),p.rom().bytes().mid(retailTitleDirectory+4,94*4));
     const auto blank=resourceBase+int(u32(image,titleDirectory+4+94*4));
     QVERIFY(blank>=0x2800000&&blank<titleDirectory);
     for(int i=94;i<32+MissionTables::keyCount;++i)
       QCOMPARE(u32(image,titleDirectory+4+i*4),quint32(blank-resourceBase));
     QCOMPARE(decodeLzss(image.mid(blank+8,u32(image,blank)-4),u32(image,blank+4)),
              QByteArray::fromHex("3634020100080016")+QByteArray(8*22+512,0));
     QCOMPARE(rebuild(reopened,tools,work.path()).image,result.image);
     auto checksum=result.image;Rom::updateChecksum(checksum);QCOMPARE(checksum,result.image);
     const auto dir=QFileInfo(out).dir().filePath(QTest::currentDataTag());QVERIFY(QDir().mkpath(dir));exportRom(p,result,QDir(dir).filePath(QFileInfo(out).fileName()));
     const auto capacity=CapacityPlacement::read(rom);
     QFile fixture(QDir(dir).filePath("campaign-fixture.json"));QVERIFY(fixture.open(QIODevice::WriteOnly));
     fixture.write(QJsonDocument(QJsonObject{{"settings",NewCampaign::settings(*p.sceneEvents())},{"mission",mission},{"capacity",capacity?double(capacity->base):0},{"arrival",QString::fromLatin1(events.program(0).toHex())},{"program",QString::fromLatin1(events.program(1).toHex())},{"army0",QString::fromLatin1(NewCampaign::armyMemory(p,NewCampaign::armyBytes(config,0)).toHex())},{"army1",QString::fromLatin1(NewCampaign::armyMemory(p,NewCampaign::armyBytes(config,1)).toHex())}}).toJson());
   }catch(const std::exception &e){QFAIL(e.what());}
 }
 void normalCampaignProbe_data(){QTest::addColumn<QString>("mode");QTest::newRow("world-only")<<QString("world");QTest::newRow("retail-map")<<QString("retail");QTest::newRow("progression")<<QString("blank");QTest::newRow("progression-map")<<QString("mapOnly");}
 void normalCampaignProbe(){
   QFETCH(QString,mode);
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"),out=qEnvironmentVariable("LORDLY_CAMPAIGN_OUTPUT");
   if(path.isEmpty()||tools.isEmpty()||out.isEmpty())QSKIP("Private ROM, compiler and owned output required");
   try {
     auto p=Project::fromRom(Rom::open(path));auto config=NewCampaign::defaults(p,6);config["goth"]=1234;config["chaos"]=37;
     QJsonObject details;
     if(mode=="blank"||mode=="mapOnly") {
       p.loadCampaignMap();auto map=*p.campaignMap();
       int removedRoad=0,removedDestination=0;QSet<int> neighbors{6};
       for(int road=1;road<=map.totalRoads();++road) {
         const auto [a,b]=map.road(road);const int other=a==6?b:b==6?a:0;
         if(other){neighbors.insert(other);removedRoad=road;removedDestination=other;}
       }
       QVERIFY(removedRoad);map.removeRoad(removedRoad);
       int nextLocation=1;
       while(nextLocation<=CampaignMap::nodeCount&&(map.position(nextLocation).isNull()||neighbors.contains(nextLocation)))++nextLocation;
       QVERIFY(nextLocation<=CampaignMap::nodeCount);
       const int addedRoad=map.addRoad(6,nextLocation);p.setCampaignMap(map.json());
       config["locations"]=QJsonArray{6,removedDestination};NewCampaign::initialize(p,config);
       const int firstKey=p.availableScenarioBattlefields().first();
       // Keep the native deployment and objective capture but shorten the walk
       // between the home and enemy headquarters in this unseeded fixture.
       p.loadWorld();p.loadMapEdits();auto edits=*p.mapEdits();const auto &source=edits.source(1);
       const auto home=source.map.points[0];const auto sites=p.world()->scenario(1).sites;
       int objective=-1;
       for(int i=0;i<p.world()->siteCount(1);++i)
         if(u8(p.world()->town(u16(sites,1+i*4)),24)==255){objective=i;break;}
       QVERIFY(objective>=0&&edits.canMoveSite(1,objective));
       const QLineF toOriginalObjective(home,source.map.points[objective]);
       QVERIFY(toOriginalObjective.length()>3);
       const auto destination=toOriginalObjective.pointAt(3/toOriginalObjective.length());
       edits.moveSite(1,objective,destination);p.setMapEdits(edits.json());
       p.createScenarioBattlefield(firstKey,1,mode,"Capture the crossing");
       const int nextKey=p.availableScenarioBattlefields().first();p.createScenarioBattlefield(nextKey,2,"mapOnly","Beyond the crossing");
       auto events=*p.sceneEvents();const auto &scenes=*p.scenePrograms();
       const int first=events.createMission(6,firstKey,"Capture the crossing",-1,-1,scenes);
       const int firstFlag=events.customFlags().last().toObject()["id"].toInt();
       const int next=events.createMission(nextLocation,nextKey,"Beyond the crossing",-1,-1,scenes,firstFlag);
       events.unlockNodeAfter(first,nextLocation,true);
       const int townFlag=events.createFlag("Reached enemy headquarters");
       auto trees=events.flagEdits()[QString::number(first)].toObject()["trees"].toArray();
       const int target=mode=="blank"?2:objective+1;
       const QJsonObject townVisit{{"type","if"},
           {"condition",QJsonObject{{"type","flag"},{"flag",townFlag},{"value",0}}},
           {"then",QJsonArray{QJsonObject{{"type","flag"},{"flag",townFlag},{"value",1}},QJsonObject{{"type","goth"},{"amount",321}}}},
           {"else",QJsonArray{}}};
       trees.append(QJsonObject{{"id","audit-town"},{"name","Arrive at enemy headquarters"},{"group",1},{"target",target},
           {"nodes",QJsonArray{townVisit}}});
       events.setTrees(first,trees,scenes);p.setSceneEvents(events.json());
       details={{"first",first},{"firstKey",firstKey},{"firstFlag",firstFlag},{"next",next},{"nextKey",nextKey},
                {"townFlag",townFlag},{"target",target},{"nextLocation",nextLocation},{"addedRoad",addedRoad},{"removedRoad",removedRoad},
                {"removedDestination",removedDestination},{"homeX",home.x()},{"homeZ",home.y()},
                {"targetX",destination.x()},{"targetZ",destination.y()}};
     } else {
       NewCampaign::initialize(p,config);
       if(mode=="retail"){p.setScenarios(Scenarios::load(p.rom()).json());auto events=*p.sceneEvents();events.createMission(6,1,"Native battlefield probe",-1,-1,*p.scenePrograms());p.setSceneEvents(events.json());}
     }
     QTemporaryDir build;const auto result=rebuild(p,tools,build.path());
     const auto dir=QFileInfo(out).dir().filePath(QTest::currentDataTag());QVERIFY(QDir().mkpath(dir));
     exportRom(p,result,QDir(dir).filePath(QFileInfo(out).fileName()));p.saveFolder(QDir(dir).filePath("project"));
     if(!details.isEmpty()){QFile fixture(QDir(dir).filePath("progression.json"));QVERIFY(fixture.open(QIODevice::WriteOnly));fixture.write(QJsonDocument(details).toJson());}
   }catch(const std::exception &e){QFAIL(e.what());}
 }
 void firstScenarioFromEmptyStoryline(){
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
   try {
     auto p=Project::fromRom(Rom::open(path));NewCampaign::initialize(p,NewCampaign::defaults(p,6));
     const auto before=p.manifest();QUndoStack undo;CampaignEditor editor(&p,&undo,[]{});
     QStringList errors;QTimer guard;QObject::connect(&guard,&QTimer::timeout,&editor,[&]{if(auto message=qobject_cast<QMessageBox *>(QApplication::activeModalWidget())){errors.append(message->text());qWarning().noquote()<<message->text();message->accept();}});guard.start(50);
     auto pane=static_cast<ScenarioEventEditor *>(editor.findChild<QWidget *>("storylineEvents"));QVERIFY(pane);pane->selectLocation(27);
     pane->findChild<QTabBar *>("storylineEventViewChoice")->setCurrentIndex(1);
     auto create=pane->findChild<QPushButton *>("storyCreateScenario");QVERIFY(create&&!create->isHidden()&&create->isEnabled());
     bool sawDialog=false;QTimer timer;QObject::connect(&timer,&QTimer::timeout,&editor,[&]{
       auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;
       auto mode=dialog->findChild<QComboBox *>("newMissionBattlefield");if(!mode)return;
       timer.stop();sawDialog=true;
       auto available=dialog->findChild<QComboBox *>("newMissionAvailability");QVERIFY(available);QCOMPARE(available->currentData().toInt(),0);
       mode->setCurrentIndex(mode->findData("blank"));dialog->findChild<QLineEdit *>("newMissionName")->setText("First campaign mission");
       dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
       if(dialog->isVisible()){for(auto label:dialog->findChildren<QLabel *>())qWarning().noquote()<<label->text();dialog->reject();}
     });timer.start(30);create->click();timer.stop();QVERIFY2(errors.isEmpty(),qPrintable(errors.join('\n')));QVERIFY(sawDialog);QCOMPARE(undo.count(),1);
     const int row=SceneEvents::firstCustomRow;QCOMPARE(p.sceneEvents()->missionName(row),QString("First campaign mission"));
     const auto context=EventContexts(p.rom(),*p.sceneEvents()).location(27);QVERIFY(!context.phases[1].isEmpty());QCOMPARE(context.phases[1].first().row,row);
     const auto writers=EventFlagLinks::writers(p.rom(),*p.sceneEvents(),*p.scenePrograms(),row,256,1);
     QVERIFY(std::any_of(writers.cbegin(),writers.cend(),[&](const auto &writer){return writer.row==row;}));
     QVERIFY(!EventScript::decode(p.sceneEvents()->program(0),0).isEmpty());
     QVERIFY(!EventFlow::read(p.rom(),*p.sceneEvents(),*p.scenePrograms(),1).isEmpty());
     QVERIFY(NewCampaign::settings(*p.sceneEvents())["locations"].toArray().contains(27));
     const auto after=p.manifest();undo.undo();QCOMPARE(p.manifest(),before);undo.redo();QCOMPARE(p.manifest(),after);
   }catch(const std::exception &e){QFAIL(e.what());}
 }
 void createScenarioFromWorldMap(){
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
   auto p=Project::fromRom(Rom::open(path));p.loadCampaignMap();p.loadSceneEvents();
   auto map=*p.campaignMap();const int location=map.availableNodes().first();
   map.setNode(location,{150,100},map.label(6));map.setName(location,"New crossing");p.setCampaignMap(map.json());
   const auto before=p.manifest();QUndoStack undo;CampaignEditor editor(&p,&undo,[]{});editor.selectNode(location);
   auto create=editor.findChild<QPushButton *>("campaignCreateScenario");QVERIFY(create&&!create->isHidden());
   bool sawDialog=false;QTimer timer;QObject::connect(&timer,&QTimer::timeout,&editor,[&]{
     auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;
     auto presets=dialog->findChild<QComboBox *>("newMissionBattlefield");if(!presets)return;
     timer.stop();sawDialog=true;QCOMPARE(presets->count(),3);presets->setCurrentIndex(presets->findData("blank"));
     auto name=dialog->findChild<QLineEdit *>("newMissionName");QVERIFY(name);name->setText("Crossing campaign");
     auto buttons=dialog->findChild<QDialogButtonBox *>();QVERIFY(buttons);
     buttons->button(QDialogButtonBox::Ok)->click();
     if(dialog->isVisible()){QStringList errors;for(auto label:dialog->findChildren<QLabel *>())if(label->isVisible())errors<<label->text();qWarning().noquote()<<errors.join("\n");dialog->reject();}
   });timer.start(30);create->click();QVERIFY(sawDialog);
   const int key=MissionTables::firstKey,row=SceneEvents::firstCustomRow;
   QCOMPARE(undo.count(),1);QCOMPARE(p.scenarioName(key),QString("Crossing campaign"));
   QCOMPARE(p.scenarios()->scenario(key).squads.size(),0);QCOMPARE(p.sceneEvents()->missionName(row),QString("Crossing campaign"));
   auto context=EventContexts(p.rom(),*p.sceneEvents()).location(location);QCOMPARE(context.phases[1].size(),1);QCOMPARE(context.phases[1].first().row,row);
   auto steps=EventFlow::read(p.rom(),*p.sceneEvents(),*p.scenePrograms(),row);QCOMPARE(steps.size(),2);
   const auto charts=scenarioChart(p,{{"Arrival",context.phases[0].first()}});
   QVERIFY(!charts.isEmpty());QCOMPARE(charts.first().title,QString("Arrival · New crossing"));
   bool namedMission=false;for(const auto &chart:charts)for(const auto &step:chart.steps){
       QVERIFY(!step.title.contains("Volmus"));namedMission|=step.title=="Load mission · Crossing campaign";
   }QVERIFY(namedMission);
   const auto compact=EventFlow::read(p.rom(),*p.sceneEvents(),*p.scenePrograms(),0,true,true,&*p.campaignMap());
   bool namedArrival=false;for(const auto &event:compact)if(event.entry==context.phases[0].first().entry){namedArrival=true;QCOMPARE(event.title,charts.first().title);}
   QVERIFY(namedArrival);
   auto rename=*p.campaignMap();rename.setName(location,"North Castle");p.setCampaignMap(rename.json());
   QCOMPARE(scenarioChart(p,{{"Arrival",context.phases[0].first()}}).first().title,QString("Arrival · North Castle"));
   undo.undo();QCOMPARE(p.manifest(),before);undo.redo();QCOMPARE(p.sceneEvents()->missionName(row),QString("Crossing campaign"));
 }
 void customMissionCampaign(){
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),toolchain=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
   if(path.isEmpty()||toolchain.isEmpty())QSKIP("US Rev 0 ROM and native toolchain required");
   try{
     const auto rom=Rom::open(path);auto p=Project::fromRom(rom);p.loadCampaignMap();p.loadSceneEvents();
     auto map=*p.campaignMap();const int location=map.availableNodes().first();
     map.setNode(location,{150,100},map.label(6));map.setName(location,"North Crossing");map.addRoad(6,location);p.setCampaignMap(map.json());
     qInfo()<<"Creating private battlefield";
     p.createScenarioBattlefield(10,1,"blank","Battle of North Crossing");
     auto events=*p.sceneEvents();const auto &scenes=*p.scenePrograms();
     const int row=events.createMission(location,10,"Battle of North Crossing",-1,-1,scenes);
     QCOMPARE(row,SceneEvents::firstCustomRow);events.unlockNodeAfter(4,location,true);
     auto trees=events.flagEdits()[QString::number(row)].toObject()["trees"].toArray();
     const int flag=events.createFlag("Met at the crossing");
     trees.append(QJsonObject{{"id","town-visit"},{"name","Meet the defenders"},{"group",2},{"target",1},
       {"nodes",QJsonArray{QJsonObject{{"type","flag"},{"flag",flag},{"value",1}}}}});
     events.setTrees(row,trees,scenes);p.setSceneEvents(events.json());
     qInfo()<<"Checking new mission routing";
     auto checkRoutes=[&](const Project &project){
        StoryRoutes routing(project.rom());routing.nodes[location]={};routing.scripts[0]=project.sceneEvents()->program(0);
        auto routes=routing.routes(location,50,{{256,0}});QCOMPARE(routes.size(),1);QCOMPARE(routes.first().event,row);
        QVERIFY(routing.routes(location,50,{{256,1}}).isEmpty());
        const auto flow=EventFlow::read(project.rom(),*project.sceneEvents(),*project.scenePrograms(),row);
        QCOMPARE(flow.size(),3);QCOMPARE(flow[0].title,QString("Start the battle"));
     };
     checkRoutes(p);QTemporaryDir work;p.saveFolder(work.filePath("project"));
     auto loaded=Project::fromRom(rom);loaded.applyManifest(Project::readProject(work.filePath("project")));loaded.loadSceneEvents();
     QCOMPARE(loaded.sceneEvents()->json(),events.json());checkRoutes(loaded);
     qInfo()<<"Exporting new mission";
     const auto built=rebuild(loaded,toolchain,work.path());const auto output=Rom::fromBytes(built.image);
     qInfo()<<"Reopening exported mission";
     auto reopened=Project::fromRom(output);reopened.loadSceneEvents();checkRoutes(reopened);
     QCOMPARE(SceneEvents::readRow(output,row),events.program(row));
     QCOMPARE(reopened.sceneEvents()->flagEdits(),events.flagEdits());
     auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);
     QCOMPARE(rebuild(reopened,toolchain,work.path()).image,built.image);
     const auto folder=qEnvironmentVariable("LORDLY_MISSION_FIXTURE_DIR");
     if(!folder.isEmpty()){
       QVERIFY(QDir().mkpath(folder));exportRom(loaded,built,QDir(folder).filePath(QFileInfo(path).fileName()));
       QFile fixture(QDir(folder).filePath("mission-fixture.json"));QVERIFY(fixture.open(QIODevice::WriteOnly));
       fixture.write(QJsonDocument(QJsonObject{{"event",row},{"battlefield",10},{"location",location},{"completionFlag",256},{"townFlag",flag},{"closingRequest",3},{"objective",2},{"mode","blank"}}).toJson());
     }
     // Reuse the authored event with an existing map, then detach its original
     // independent battlefield. Only the shared-map completion hook survives.
     auto sharedEvents=*reopened.sceneEvents();
     auto sharedTrees=sharedEvents.flagEdits()[QString::number(row)].toObject()["trees"].toArray();
     auto opening=sharedTrees[0].toObject();auto steps=opening["nodes"].toArray();
     auto prepare=steps[0].toObject();prepare["battlefield"]=1;steps.replace(0,prepare);
     opening["nodes"]=steps;sharedTrees.replace(0,opening);
     sharedEvents.setTrees(row,sharedTrees,*reopened.scenePrograms());reopened.setSceneEvents(sharedEvents.json());
     auto detached=reopened.manifest();detached["scenarioBattlefields"]=QJsonArray{};
     reopened.applyManifest(detached);
     QTemporaryDir detachWork;QVERIFY(detachWork.isValid());
     const auto sharedBuild=rebuild(reopened,toolchain,detachWork.path());
     const auto sharedRom=Rom::fromBytes(sharedBuild.image);
     auto sharedProject=Project::fromRom(sharedRom);sharedProject.loadSceneEvents();
     QVERIFY(sharedProject.scenarioBattlefields().isEmpty());
     QCOMPARE(sharedProject.sceneEvents()->program(row),sharedEvents.program(row));
     for(int at:missionBriefingCalls) QCOMPARE(u32(sharedBuild.image,at),u32(rom.bytes(),at));
     int predicate=-1;
     for(int at=0x1390f0;at<0x139c10;at+=4)
       if(u32(rom.bytes(),at)==0x0c079534U){QCOMPARE(predicate,-1);predicate=at;}
     QVERIFY(predicate>=0);QCOMPARE(u32(sharedBuild.image,predicate),u32(rom.bytes(),predicate));
     QVERIFY(sharedBuild.image.mid(missionExitDispatch,16)!=missionExitDispatchCode());
     QCOMPARE(canonicalMissionImage(sharedRom).mid(missionExitDispatch,16),missionExitDispatchCode());
     QCOMPARE(rebuild(sharedProject,toolchain,detachWork.path()).image,sharedBuild.image);
   }catch(const std::exception &e){QFAIL(e.what());}
 }
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
 void groupedActionsAndRecruitmentPresets() {
   const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
   if(path.isEmpty())QSKIP("ROM required");
   try {
     auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();
     auto original=tree();
     original["nodes"]=QJsonArray{QJsonObject{{"type","recruit"},{"template",11},{"mode",1}}};
     QTimer::singleShot(0,[]{
       auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
       QTimer::singleShot(15000,dialog,&QDialog::reject);
       auto character=dialog->findChild<QComboBox *>("eventRecruitCharacter");QVERIFY(character);
       QCOMPARE(character->currentData().toInt(),6);
       int berserkers=0;for(int i=0;i<character->count();++i)berserkers+=character->itemData(i).toInt()==6;
       QCOMPARE(berserkers,1);
       auto preset=dialog->findChild<QComboBox *>("eventRecruitPreset");QVERIFY(preset);
       QCOMPARE(preset->currentData().toInt(),11);
       dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
     });
     const auto unchanged=editEventTree(&project,10,original,nullptr);
     QVERIFY(unchanged);QCOMPARE((*unchanged)["nodes"],original["nodes"]);
     QTimer::singleShot(0,[]{
       auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
       QTimer::singleShot(15000,dialog,&QDialog::reject);
       QTimer::singleShot(0,[]{
         auto picker=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(picker);
         QTimer::singleShot(15000,picker,&QDialog::reject);
         auto groups=picker->findChild<QComboBox *>("eventActionGroup");QVERIFY(groups);
         auto search=picker->findChild<QLineEdit *>("eventActionSearch");QVERIFY(search);
         auto actions=picker->findChild<QTableWidget *>("eventActionList");QVERIFY(actions);
         auto buttons=picker->findChild<QDialogButtonBox *>();QVERIFY(buttons);
         groups->setCurrentIndex(groups->findData("Characters and squads"));
         search->setText("no such action");QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
         search->setText("recruit");actions->sortItems(0,Qt::DescendingOrder);
         int visible=0;
         for(int r=0;r<actions->rowCount();++r)if(!actions->isRowHidden(r)) {
           ++visible;QCOMPARE(actions->item(r,0)->data(Qt::UserRole).toString(),QString("recruit"));
           actions->setCurrentCell(r,0);
         }
         QCOMPARE(visible,1);QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
         buttons->button(QDialogButtonBox::Ok)->click();
       });
       dialog->findChild<QPushButton *>("eventTreeAddAction")->click();
       QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
       auto character=dialog->findChild<QComboBox *>("eventRecruitCharacter");QVERIFY(character);
       const int index=character->findData(6);QVERIFY(index>=0);character->setCurrentIndex(index);
       QMetaObject::invokeMethod(character,"activated",Q_ARG(int,index));
       QTRY_COMPARE(dialog->findChild<QComboBox *>("eventRecruitPreset")->currentData().toInt(),1);
       dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
     });
     const auto edited=editEventTree(&project,10,original,nullptr);QVERIFY(edited);
     const auto nodes=(*edited)["nodes"].toArray();QCOMPARE(nodes.size(),2);
     QCOMPARE(nodes[0].toObject()["template"].toInt(),11);
     QCOMPARE(nodes[1].toObject()["template"].toInt(),1);
     auto events=*project.sceneEvents();events.setTrees(10,{*edited},*project.scenePrograms());
     project.setSceneEvents(events.json());
     auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(project.manifest());
     QCOMPARE(reopened.sceneEvents()->program(10),project.sceneEvents()->program(10));
   }catch(const std::exception &e){QFAIL(e.what());}
 }
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
       QTimer::singleShot(0,[]{auto picker=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(picker);QTimer::singleShot(15000,picker,&QDialog::reject);auto actions=picker->findChild<QTableWidget *>("eventActionList");QVERIFY(actions);for(int r=0;r<actions->rowCount();++r)if(actions->item(r,0)->data(Qt::UserRole)=="flag"){actions->setCurrentCell(r,0);picker->accept();return;}QFAIL("Flag action missing");});
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
