#include "app/scenario_event_editor.h"
#include "app/event_flow_editor.h"
#include "app/scenario_flowchart.h"
#include "engine/event_flag_links.h"
#include "engine/event_authoring.h"
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QDialogButtonBox>
#include <QDialog>
#include <QMessageBox>
#include <QTabBar>
#include <QTimer>
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
    void standardClosingVisible(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();
        const auto automatic=missionEndFlow(project);QVERIFY(automatic.automatic);QVERIFY(automatic.steps.size()>20);
        const auto node=[&](QString text){for(int i=0;i<automatic.steps.size();++i)if(automatic.steps[i].title.startsWith(text))return i;return -1;};
        const auto edge=[&](int from,QString label,int to){return std::any_of(automatic.edges.cbegin(),automatic.edges.cend(),[&](const auto &e){return e.from==from&&e.to==to&&e.label==label;});};
        const int zombie=node("Zombie roll"),barrier=node("Character is protected"),next=node("Move to the next");
        QVERIFY(zombie>=0&&barrier>=0&&next>=0);
        QVERIFY(edge(node("Angel Knight roll"),"No",zombie));
        QVERIFY(edge(node("Angel Knight roll"),"Yes",node("Change current class to Angel Knight")));
        QVERIFY(edge(zombie,"Yes",barrier));QVERIFY(edge(barrier,"Yes",next));
        QVERIFY(edge(barrier,"No",node("Change current class to Zombie")));
        QVERIFY(edge(node("Character is a squad leader"),"Yes",next));
        QVERIFY(edge(node("Both identity class"),"No",next));
        for(const auto &step:automatic.steps){QVERIFY(step.address.row<0);QVERIFY(step.instructions.isEmpty());}
        for(const auto &e:automatic.edges)QVERIFY(e.from>=0&&e.from<automatic.steps.size()&&e.to>=0&&e.to<automatic.steps.size());
        auto retail=scenarioChart(project,{{"Mission",{4,-1,{}}}});
        QCOMPARE(std::count_if(retail.cbegin(),retail.cend(),[](const auto &e){return e.automatic;}),1);
        auto events=*project.sceneEvents();const int row=events.createMission(6,1,"Closing example",-1,17,*project.scenePrograms());
        project.setSceneEvents(events.json());const auto before=project.manifest();const auto program=events.program(row);
        auto chart=scenarioChart(project,{{"Mission",{row,-1,{}}}});
        QCOMPARE(std::count_if(chart.cbegin(),chart.cend(),[](const auto &e){return e.automatic;}),1);
        auto shared=std::find_if(chart.cbegin(),chart.cend(),[](const auto &e){return e.automatic;});
        QCOMPARE(shared->steps.size(),automatic.steps.size());
        const auto flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),row);
        auto closing=std::find_if(flow.cbegin(),flow.cend(),[](const auto &e){return e.group==5;});QVERIFY(closing!=flow.cend());
        QUndoStack undo;EventFlowEditor editor(&project,&undo,[]{});editor.setRow(row);editor.selectOffset(closing->entry);
        auto common=editor.findChild<QTreeWidget *>("storyMissionEndTree");QVERIFY(common);
        QVERIFY(!common->findItems("*Zombie roll*",Qt::MatchWildcard|Qt::MatchRecursive).isEmpty());
        auto authored=editor.findChild<QTreeWidget *>("storyAuthoredTree");QVERIFY(authored);
        QCOMPARE(authored->topLevelItemCount(),3);
        QVERIFY(authored->topLevelItem(0)->text(0).startsWith("Play cutscene"));
        QVERIFY(authored->topLevelItem(2)->text(0).contains("Finish mission and return"));
        QCOMPARE(project.manifest(),before);QCOMPARE(project.sceneEvents()->program(row),program);QCOMPARE(undo.count(),0);
        if(const auto capture=qEnvironmentVariable("LORDLY_CLOSING_SCREENSHOT");!capture.isEmpty()){
            editor.resize(1400,1050);editor.show();QApplication::processEvents();QVERIFY(editor.grab().save(capture));
        }
        ScenarioFlowchart canvas(&project);int edits=0;
        canvas.edit=[&](ChartAddress){++edits;};canvas.setScenario("closing-example",chart);canvas.resize(1400,950);canvas.show();
        for(auto button:canvas.findChildren<QPushButton *>())if(button->text()=="Expand all")button->click();
        QApplication::processEvents();auto view=canvas.findChild<QGraphicsView *>("scenarioFlowCanvas");QVERIFY(view);
        const int commonIndex=int(shared-chart.cbegin());
        for(auto item:view->scene()->items())if(item->data(0)=="event"&&item->data(1).toInt()==commonIndex){
            view->centerOn(item);QApplication::processEvents();
            QTest::mouseDClick(view->viewport(),Qt::LeftButton,{},view->mapFromScene(item->sceneBoundingRect().center()));break;
        }
        QCOMPARE(edits,0);
        for(auto button:canvas.findChildren<QPushButton *>())if(button->text()=="Add step"||button->text().startsWith("Delete"))QVERIFY(!button->isEnabled());
        if(const auto capture=qEnvironmentVariable("LORDLY_CLOSING_SCREENSHOT");!capture.isEmpty()){
            for(auto item:view->scene()->items())if(item->data(0)=="step"&&item->data(1).toInt()==commonIndex&&item->data(2).toInt()==zombie){view->centerOn(item);break;}
            QApplication::processEvents();QVERIFY(canvas.grab().save(capture+"-chart.png"));
        }
        QCOMPARE(project.manifest(),before);QCOMPARE(project.sceneEvents()->program(row),program);
        // Unknown altered native code must not silently acquire the stock chart.
        auto bytes=project.rom().bytes();bytes[0x1b676b]=4;
        auto modified=Project::fromRom(Rom::fromBytes(bytes));
        const auto unknown=missionEndFlow(modified);QCOMPARE(unknown.steps.size(),1);QVERIFY(unknown.steps.first().title.contains("modified"));
    }
    void standardClosingReopened(){
        const auto path=qEnvironmentVariable("LORDLY_CLOSING_REOPEN_ROM");if(path.isEmpty())QSKIP("Exported campaign ROM required");
        const auto project=Project::fromRom(Rom::open(path));
        const auto graph=missionEndFlow(project);QVERIFY(graph.automatic);QVERIFY(graph.steps.size()>20);
    }
    void allProgramCharts(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();
        const auto before=project.manifest();int programs=0;
        for(int row=0;row<115;++row){
            const auto offsets=EventScript::instructionOffsets(row);if(offsets.isEmpty())continue;
            const auto chart=scenarioChart(project,{{"Program",{row,-1,{}}}});
            QSet<int> owned;
            for(const auto &event:chart){
                for(const auto &step:event.steps)for(int pc:step.instructions)owned.insert(pc);
                for(const auto &edge:event.edges)
                    QVERIFY(edge.from>=0&&edge.to>=0&&edge.from<event.steps.size()&&edge.to<event.steps.size());
            }
            QVERIFY2(owned==offsets,qPrintable(QString("Chart omitted instructions from program %1 (%2/%3 mapped)").arg(row).arg(owned.size()).arg(offsets.size())));
            ++programs;
        }
        QCOMPARE(programs,113);QCOMPARE(project.manifest(),before);
    }
    void originalConditionFlowchart(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();
        const int flag=events.createFlag("Visited the northern crossing");
        QJsonObject gate{{"mode","if"},{"condition",QJsonObject{{"type","flag"},{"flag",flag},{"value",1}}},{"yes","original"},{"no","no"}};
        events.setRetailGate(10,0x7e,gate,*project.scenePrograms());project.setSceneEvents(events.json());
        auto model=scenarioChart(project,{{"Mission",{10,-1,{}}}});
        bool visible=false;
        for(const auto &event:model)for(int i=0;i<event.steps.size();++i){
            const auto &step=event.steps[i];if(!step.address.gateControl||step.address.gate!=0x7e)continue;
            visible=true;QVERIFY(step.title.contains("Visited the northern crossing"));QVERIFY(step.decision);
            bool yes=false,no=false;
            for(const auto &edge:event.edges)if(edge.from==i){yes|=edge.label=="Yes"&&event.steps[edge.to].address.gate==0x7e&&!event.steps[edge.to].address.gateControl;no|=edge.label=="No";}
            QVERIFY(yes);QVERIFY(no);
        }
        QVERIFY(visible);
        QUndoStack undo;EventFlowEditor editor(&project,&undo,[]{});editor.setRow(10);editor.selectOffset(0x7e);
        auto compact=editor.findChild<QTreeWidget *>("storyCompactTree");QVERIFY(compact);
        QVERIFY(!compact->findItems("*Visited the northern crossing*",Qt::MatchWildcard|Qt::MatchRecursive).isEmpty());
        const auto native=EventFlow::read(project.rom(),events,*project.scenePrograms(),10,true,false);
        bool edited=false;
        for(const auto &event:native)if(event.triggerInstructions.contains(0x7e)){
            QVERIFY(!event.triggerFields.isEmpty());EventFlow::set(events,*project.scenePrograms(),10,event.triggerFields.first(),7);edited=true;break;
        }
        QVERIFY(edited);QCOMPARE(u8(events.program(10,false),0x7d),7);QCOMPARE(u8(events.program(10),0x7e),8);
        events.setRetailGate(10,0x7e,{{"mode","keep"},{"outcome","no"}},*project.scenePrograms());
        project.setSceneEvents(events.json());model=scenarioChart(project,{{"Mission",{10,-1,{}}}});
        visible=false;for(const auto &event:model)for(const auto &step:event.steps)if(step.address.gate==0x7e){
            visible=true;QVERIFY(!step.decision);QVERIFY(step.title.contains("No"));
        }QVERIFY(visible);
        events.setRetailGate(10,0x7e,gate,*project.scenePrograms());project.setSceneEvents(events.json());
        ScenarioEventEditor scenario(&project,&undo,[]{});scenario.selectScenario(7);
        auto view=scenario.findChild<QGraphicsView *>("scenarioFlowCanvas");QVERIFY(view);
        ScenarioFlowchart *chart=nullptr;
        for(auto parent=view->parentWidget();parent&&!chart;parent=parent->parentWidget())chart=dynamic_cast<ScenarioFlowchart *>(parent);
        QVERIFY(chart);
        QTimer chooseBranch;
        QObject::connect(&chooseBranch,&QTimer::timeout,&scenario,[&]{
            auto dialog=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(!dialog)return;
            for(auto button:dialog->buttons())if(button->text()=="Keep Yes"){button->click();return;}
            qWarning().noquote()<<dialog->text();dialog->reject();
        });
        ChartAddress address{10,0x7a};address.gate=0x7e;
        chooseBranch.start(30);chart->editGate(address,2,true);chooseBranch.stop();
        const auto retained=project.sceneEvents()->flagEdits()["10"].toObject()["gates"].toObject()["126"].toObject();
        QCOMPARE(retained["mode"].toString(),QString("if"));QCOMPARE(retained["condition"],gate["condition"]);
        QCOMPARE(retained["yes"].toString(),QString("yes"));QCOMPARE(retained["no"].toString(),QString("no"));
        const auto removed=scenarioChart(project,{{"Mission",{10,-1,{}}}});
        for(const auto &event:removed){
            for(const auto &step:event.steps)if(step.address.gate==0x7e)QVERIFY(step.address.gateControl);
            for(const auto &edge:event.edges)QVERIFY(edge.from>=0&&edge.to>=0&&edge.from<event.steps.size()&&edge.to<event.steps.size());
        }
        undo.undo();QCOMPARE(project.sceneEvents()->flagEdits()["10"].toObject()["gates"].toObject()["126"].toObject(),gate);
        undo.redo();QCOMPARE(project.sceneEvents()->flagEdits()["10"].toObject()["gates"].toObject()["126"].toObject(),retained);
    }
    void scenarioFlowchartNavigation(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();
        const int flag=events.createFlag("Chart test");
        QJsonObject condition{{"type","flag"},{"flag",flag},{"value",0}};
        QJsonObject write{{"type","flag"},{"flag",flag},{"value",1}};
        QJsonObject give{{"type","goth"},{"amount",25}};
        QJsonObject decision{{"type","if"},{"condition",condition},{"then",QJsonArray{write}},{"else",QJsonArray{give}}};
        QJsonObject tree{{"id","chart-fixture"},{"name","Cayes choice"},{"group",1},{"target",6},{"nodes",QJsonArray{decision}}};
        events.setTrees(10,{tree},*project.scenePrograms());project.setSceneEvents(events.json());const auto before=project.manifest();
        auto model=scenarioChart(project,{{"Mission",{10,-1,EventContexts::programName(10)}}});
        const auto offer=std::find_if(model.cbegin(),model.cend(),[](const auto &e){return e.address.offset==0x4a;});
        QVERIFY(offer!=model.cend());
        auto transition=[&](int from,QString arm,int to){
            for(const auto &link:offer->edges)if(offer->steps[link.from].instructions.contains(from)&&link.label==arm&&offer->steps[link.to].instructions.contains(to))return true;
            return false;
        };
        QVERIFY(transition(0x4a,"Yes",0x7a)); // Visitor qualifies, then test the town.
        QVERIFY(transition(0x7a,"Yes",0x82)); // At Cayes, then test flag 202.
        QVERIFY(transition(0x82,"Yes",0x8e)); // First visit plays the offer.
        QVERIFY(transition(0x98,"Yes",0xba)); // The selected answer is the refusal.
        QVERIFY(transition(0x98,"No",0xa0));
        auto authored=std::find_if(model.begin(),model.end(),[](const auto &e){return e.address.tree=="chart-fixture";});QVERIFY(authored!=model.end());
        QCOMPARE(authored->steps.size(),5);QCOMPARE(authored->steps[1].flags.first().use,QString("Checks"));
        QCOMPARE(authored->steps[2].flags.first().use,QString("Sets ON"));QCOMPARE(authored->steps[3].address.path,(QVector<int>{0,2,0}));
        auto edge=[&](int from,int to,QString label){return std::any_of(authored->edges.begin(),authored->edges.end(),[&](const auto &v){return v.from==from&&v.to==to&&v.label==label;});};
        QVERIFY(edge(1,2,"Yes"));QVERIFY(edge(1,3,"No"));QVERIFY(edge(2,4,{}));QVERIFY(edge(3,4,{}));
        const auto links=EventFlagLinks::references(project.rom(),events,*project.scenePrograms(),4,flag);
        QCOMPARE(links.size(),2);
        for(const auto &hit:links){QCOMPARE(hit.row,10);QCOMPARE(hit.authoredId,tree["id"].toString());
            QCOMPARE(EventAuthoring::stepPath(tree,hit.offset-hit.entry),hit.read?(QVector<int>{0,0}):(QVector<int>{0,1,0}));}
        QUndoStack undo;ScenarioEventEditor editor(&project,&undo,[]{});editor.selectScenario(7);editor.resize(1450,900);editor.show();QApplication::processEvents();
        auto modes=editor.findChild<QTabBar *>("scenarioEventViewChoice");QVERIFY(modes);QCOMPARE(modes->currentIndex(),0);
        QVERIFY(editor.findChild<QTreeWidget *>("storyCompactTree"));modes->setCurrentIndex(1);QApplication::processEvents();
        auto view=editor.findChild<QGraphicsView *>("scenarioFlowCanvas");QVERIFY(view&&view->isVisible());
        auto items=[&](QString kind){QList<QGraphicsItem *> out;for(auto item:view->scene()->items())if(item->data(0).toString()==kind)out<<item;return out;};
        auto card=[&](QString title)->int{for(auto e:items("event"))for(auto child:e->childItems()){
            if(auto text=qgraphicsitem_cast<QGraphicsTextItem *>(child);text&&text->toPlainText().contains(title))return e->data(1).toInt();
        }return -1;};
        auto click=[&](QString kind,int event,int step,int id,bool twice){
            for(auto item:items(kind))if(item->data(1).toInt()==event&&(step<0||item->data(2).toInt()==step)&&(id<0||item->data(3).toInt()==id)){
                view->centerOn(item);QApplication::processEvents();auto rect=item->sceneBoundingRect();
                const auto point=view->mapFromScene(QPointF(rect.center().x(),rect.top()+9));
                if(twice)QTest::mouseDClick(view->viewport(),Qt::LeftButton,{},point);
                else QTest::mouseClick(view->viewport(),Qt::LeftButton,{},point);
                QApplication::processEvents();return true;
            }return false;
        };
        const int custom=card("Cayes choice");QVERIFY(custom>=0);QVERIFY(click("event",custom,-1,-1,false));
        QVERIFY(click("flag",custom,1,flag,false));auto uses=editor.findChild<QListWidget *>("scenarioFlowFlagUses");QVERIFY(uses->isVisible());QCOMPARE(uses->count(),2);
        QCOMPARE(project.manifest(),before);QCOMPARE(undo.count(),0);
        QTimer editDialog;QObject::connect(&editDialog,&QTimer::timeout,&editor,[&]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;
            editDialog.stop();
            auto steps=dialog->findChild<QTreeWidget *>("eventTreeSteps");
            auto item=steps?steps->currentItem():nullptr;const bool correct=item&&item->text(0).contains("25")&&item->parent()&&item->parent()->text(0)=="ELSE";
            if(correct){auto amount=dialog->findChild<QSpinBox *>();if(amount)amount->setValue(35);dialog->accept();}else dialog->reject();
        });editDialog.start(30);
        QVERIFY(click("step",custom,3,-1,true));QCOMPARE(modes->currentIndex(),1);QCOMPARE(undo.count(),1);
        const auto edited=project.sceneEvents()->flagEdits()["10"].toObject()["trees"].toArray().first().toObject();
        QCOMPARE(edited["nodes"].toArray().first().toObject()["else"].toArray().first().toObject()["amount"].toInt(),35);
        undo.undo();QCOMPARE(project.manifest(),before);
        modes->setCurrentIndex(1);QApplication::processEvents();
        const int cayes=card("Katreda asks");QVERIFY(cayes>=0);QVERIFY(click("event",cayes,-1,-1,false));
        QVERIFY(click("flag",cayes,-1,202,false));QVERIFY(uses->count()>=3);
        for(auto type:{"Checks","Sets ON"}){bool found=false;for(int i=0;i<uses->count();++i)found|=uses->item(i)->text().startsWith(type);QVERIFY(found);}
        if(auto capture=qEnvironmentVariable("LORDLY_FLOW_SCREENSHOT");!capture.isEmpty())QVERIFY(editor.grab().save(capture));
        int writer=-1;for(int i=0;i<uses->count();++i)if(uses->item(i)->text().startsWith("Sets ON"))writer=i;
        QVERIFY(writer>=0);uses->itemActivated(uses->item(writer));QCOMPARE(modes->currentIndex(),0);
        auto compact=editor.findChild<QScrollArea *>("storyEventDetails")->widget()->findChild<QTreeWidget *>("storyCompactTree");
        QVERIFY(compact&&compact->currentItem()->text(0).contains("202"));
        QCOMPARE(project.manifest(),before);
    }
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
            auto outline=editor.findChild<QScrollArea *>("storyEventDetails")->widget()->findChild<QTreeWidget *>("storyCompactTree");
            const bool worldMapOutcome=outline&&!outline->findItems("Stay on the world map",Qt::MatchContains|Qt::MatchRecursive).isEmpty();
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
            QCOMPARE(findField(branch(offer,352),352,"comparison").label,QString("Squad roster"));
            QVERIFY(!branch(offer,352).title.contains("Liedel"));
            EventFlow::set(events,*project.scenePrograms(),16,squad,31);QCOMPARE(u8(events.program(16),333),quint8(31));
            EventFlow::set(events,*project.scenePrograms(),16,findField(branch(offer,346),340,"value"),1);
            QCOMPARE(u8(events.program(16),341),quint8(1));
            EventFlow::set(events,*project.scenePrograms(),16,findField(branch(offer,382),382,"comparison"),13);
            EventFlow::set(events,*project.scenePrograms(),16,findField(branch(offer,382),380,"value"),67);
            QCOMPARE(u16(events.program(16),382),quint16(0xd102));
            flow=EventFlow::read(project.rom(),events,*project.scenePrograms(),16);offer=entry(flow,330);
            QCOMPARE(branch(offer,382).title,QString("Chaos Frame < 67"));QVERIFY(branch(offer,346).title.contains("Second member"));
            project.setSceneEvents(events.json());QTemporaryDir work;project.saveFolder(work.filePath("project"));
            auto reopened=Project::fromRom(project.rom());reopened.applyManifest(Project::readProject(work.filePath("project")));reopened.loadSceneEvents();
            QCOMPARE(reopened.sceneEvents()->program(16),events.program(16));
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void namedConditionsUseCurrentInstructions(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();auto events=*project.sceneEvents();
        const auto &scenes=*project.scenePrograms();
        auto read=[&](int row){return EventFlow::read(project.rom(),events,scenes,row);};
        auto at=[](const QVector<StoryEvent> &flow,int pc){
            for(const auto &e:flow)for(const auto &a:e.actions)if(a.instructions.contains(pc))return a;
            throw Error("Missing expected retail event instruction");
        };
        auto gunther=read(10);
        auto condition=at(gunther,278);QCOMPARE(condition.title,QString("Visiting squad uses the opposing roster"));
        QCOMPARE(condition.target,330);QCOMPARE(condition.otherwise,280);QCOMPARE(condition.condition.fields.size(),1);
        const auto roster=condition.condition.fields.first();
        QCOMPARE(roster.label,QString("Squad roster"));QCOMPARE(roster.value,14);QVERIFY(roster.choices[13].contains("0–29"));
        const auto original=events.program(10);EventFlow::set(events,scenes,10,roster,13);
        auto expected=original;put8(expected,278,0xd6);QCOMPARE(events.program(10),expected);
        QCOMPARE(at(read(10),278).title,QString("Visiting squad uses the player army roster"));
        events.editParameter(10,276,"value",31,scenes);
        condition=at(read(10),278);QVERIFY(condition.title.contains("slot < 31"));QVERIFY(condition.condition.fields.first().label.contains("cutoff"));
        const auto katreda=at(gunther,610);
        QVERIFY(katreda.instructions.contains(608));QVERIFY(katreda.title.contains("Katreda"));
        QVERIFY(std::any_of(katreda.condition.fields.cbegin(),katreda.condition.fields.cend(),[](const auto &f){return f.label=="Shared check"&&f.value==1190;}));
        QCOMPARE(at(gunther,610).target,624);QCOMPARE(at(gunther,618).target,656);QVERIFY(at(gunther,618).title.contains("Seraph"));
        const auto scan=at(gunther,280);
        QVERIFY(std::any_of(scan.fields.cbegin(),scan.fields.cend(),[](const auto &f){return f.offset==280&&f.value==0&&f.choices.contains(4);}));
        const auto helper=entry(gunther,1190);QCOMPARE(helper.actions.size(),1);QCOMPARE(helper.actions[0].instructions.size(),48);
        const auto investigation=read(96);
        QCOMPARE(at(investigation,244).title,QString("Clock time < 06:00"));
        QVERIFY(at(investigation,396).title.contains("no inventory entry"));QVERIFY(at(investigation,402).title.startsWith("99 ≤ Owned copies"));
        events.editParameter(96,392,"value",510,scenes);QVERIFY(!at(read(96),396).title.contains("no inventory entry"));
        QVERIFY(at(read(66),4100).title.contains("0–2"));QVERIFY(at(read(54),868).title.contains("locked"));
        const auto intro=read(1);const auto reveal=at(intro,2318);
        QVERIFY(reveal.title.contains("Tenne"));QVERIFY(reveal.instructions.size()>1);
        QVERIFY(std::any_of(reveal.fields.cbegin(),reveal.fields.cend(),[](const auto &f){return f.label=="World location"&&f.choices.contains(f.value);}));
        const auto gift=at(read(71),258);
        auto fallback=std::find_if(gift.condition.fields.cbegin(),gift.condition.fields.cend(),[](const auto &f){return f.offset==258&&f.value==0;});
        QVERIFY(fallback!=gift.condition.fields.cend());QVERIFY(fallback->label.contains("no inventory entry"));
        EventFlow::set(events,scenes,71,*fallback,2);QCOMPARE(u8(events.program(71),259),quint8(2));
        const auto squadOverride=at(read(35),560).fields.first();
        QCOMPARE(squadOverride.value,290);QVERIFY(squadOverride.choices.contains(0x8122));
        const auto originalOverride=events.program(35);
        EventFlow::set(events,scenes,35,squadOverride,0x8122);
        auto leaderOnly=originalOverride;put8(leaderOnly,559,0x81);QCOMPARE(events.program(35),leaderOnly);
        QCOMPARE(at(read(35),560).fields.first().value,0x8122);
        const auto guard=at(read(68),226).fields.first();QVERIFY(guard.choices.contains(0));
        EventFlow::set(events,scenes,68,guard,0);QCOMPARE(at(read(68),226).fields.first().value,0);
        QUndoStack undo;EventFlowEditor editor(&project,&undo,[&]{editor.setRow(10);});
        editor.setRow(10);editor.resize(1450,950);editor.show();editor.selectOffset(610);QApplication::processEvents();
        auto check=editor.findChild<QPushButton *>("storyOpenCheck_1190");QVERIFY(check&&check->isVisible());
        const auto labels=editor.findChildren<QLabel *>();
        QVERIFY(std::any_of(labels.cbegin(),labels.cend(),[](QLabel *l){return l->isVisible()&&l->text().contains("Katreda");}));
        check->click();QApplication::processEvents();
        QCOMPARE(editor.findChild<QListWidget *>("storyEventList")->currentItem()->data(Qt::UserRole).toInt(),1190);
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
            EventFlow::set(events,*project.scenePrograms(),4,visit.triggerFields[0],3);EventFlow::set(events,*project.scenePrograms(),4,visit.actions[0].fields[0],611);
            const auto kinseya=entry(EventFlow::read(rom,events,*project.scenePrograms(),10),262);
            auto roster=std::find_if(kinseya.actions.cbegin(),kinseya.actions.cend(),[](const auto &a){return a.instructions.contains(278);});
            QVERIFY(roster!=kinseya.actions.cend());QCOMPARE(roster->condition.fields.first().label,QString("Squad roster"));
            EventFlow::set(events,*project.scenePrograms(),10,roster->condition.fields.first(),13);
            project.setSceneEvents(events.json());
            QTemporaryDir work;project.saveFolder(work.filePath("project"));auto loaded=Project::fromRom(rom);loaded.applyManifest(Project::readProject(work.filePath("project")));
            const auto built=rebuild(loaded,tools,work.path());auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);
            const auto output=Rom::fromBytes(built.image);auto reopened=Project::fromRom(output);reopened.loadSceneEvents();
            QCOMPARE(SceneEvents::readRow(output,4),events.program(4));QCOMPARE(reopened.sceneEvents()->json(),events.json());
            QCOMPARE(SceneEvents::readRow(output,10),events.program(10));QCOMPARE(u16(events.program(10),278),quint16(0xd619));
            const auto reopenedVisit=entry(EventFlow::read(output,*reopened.sceneEvents(),*reopened.scenePrograms(),10,false),262);
            QVERIFY(std::any_of(reopenedVisit.actions.cbegin(),reopenedVisit.actions.cend(),[](const auto &a){return a.title=="Visiting squad uses the player army roster";}));
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
