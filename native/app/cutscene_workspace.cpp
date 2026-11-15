#include "app/cutscene_editor.h"
#include "app/scene_preview.h"
#include "app/scene_action_editor.h"
#include "app/scene_timeline.h"
#include "app/scene_key_picker.h"
#include "engine/project.h"
#include "engine/event_flow.h"
#include "app/event_tree_dialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QUndoStack>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>
namespace lordly {
namespace {
class SceneTriggerCommand final : public QUndoCommand {
  public:
    SceneTriggerCommand(Project *project,QJsonObject after):QUndoCommand("Add cutscene trigger"),project_(project),before_(project->sceneEvents()->json()),after_(std::move(after)){}
    void undo() override {project_->setSceneEvents(before_);}
    void redo() override {project_->setSceneEvents(after_);}
  private:
    Project *project_;QJsonObject before_,after_;
};
}
void CutsceneEditor::createWorkspace(QTabWidget *pages){
    if(!project_||!actionEditor_)return;
    eventRows_=new QComboBox;eventRows_->setObjectName("cutsceneEventRow");
    eventRows_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);eventRows_->setMinimumContentsLength(20);
    refreshEventRows();
    static_cast<QVBoxLayout *>(scenes_->parentWidget()->layout())->insertWidget(0,eventRows_);
    connect(eventRows_,&QComboBox::currentIndexChanged,this,&CutsceneEditor::filterScenes);
    auto empty=new QLabel("No cutscenes match this event row and search.",pages->parentWidget());
    empty->setObjectName("cutsceneEmptySelection");empty->setWordWrap(true);empty->hide();
    static_cast<QVBoxLayout *>(pages->parentWidget()->layout())->insertWidget(0,empty);
    workspace_=new QWidget;workspace_->setObjectName("cutsceneWorkspace");
    auto layout=new QVBoxLayout(workspace_);auto tools=new QHBoxLayout;
    auto tool=new QComboBox;tool->setObjectName("scenePlacementTool");tool->addItems({"Place","Move"});
    auto duration=new QDoubleSpinBox;duration->setObjectName("sceneMoveSeconds");duration->setRange(1,1800);duration->setDecimals(0);duration->setValue(30);duration->setSuffix(" updates");
    auto wait=new QCheckBox("Wait until finished");wait->setObjectName("sceneMoveWait");
    tools->addWidget(tool);tools->addWidget(new QLabel("Move duration"));tools->addWidget(duration);tools->addWidget(wait);
    auto trigger=new QPushButton("Add event trigger…");trigger->setObjectName("sceneCreateTrigger");tools->addWidget(trigger);
    connect(trigger,&QPushButton::clicked,this,&CutsceneEditor::createSceneTrigger);
    auto updateTool=[=,this]{scenePreview_->clearAssetPlacement();actionEditor_->setPlacementMode(tool->currentIndex()==1,qRound(duration->value()),wait->isChecked());};
    connect(tool,&QComboBox::currentIndexChanged,this,updateTool);connect(duration,&QDoubleSpinBox::valueChanged,this,updateTool);connect(wait,&QCheckBox::toggled,this,updateTool);
    auto action=new QComboBox;action->setObjectName("sceneWorkspaceAction");
    action->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);action->setMinimumContentsLength(16);
    const auto kinds=SceneActions::kinds();for(auto [kind,name]:kinds.asKeyValueRange())action->addItem(name,kind);
    auto add=new QPushButton("Add step");add->setObjectName("sceneWorkspaceAdd");
    auto where=new QComboBox;where->setObjectName("sceneWorkspaceInsertWhere");where->addItems({"At playhead","Before selected step","After selected step","Choice: option 1","Choice: option 2"});
    auto actionTools=new QHBoxLayout;actionTools->addWidget(action,1);actionTools->addWidget(where);actionTools->addWidget(add);
    connect(add,&QPushButton::clicked,this,[=,this]{actionEditor_->addFromWorkspace(action->currentData().toUInt(),where->currentIndex());pages->setCurrentWidget(actionEditor_);});
    layout->addLayout(tools);layout->addLayout(actionTools);
    auto scroll=new QScrollArea;scroll->setObjectName("cutsceneWorkspaceScroll");scroll->setWidgetResizable(true);
    auto body=new QWidget;auto bodyLayout=new QVBoxLayout(body);bodyLayout->setContentsMargins(0,0,0,0);
    bodyLayout->setSizeConstraint(QLayout::SetMinimumSize);
    auto upper=new QWidget;auto upperLayout=new QHBoxLayout(upper);upperLayout->setContentsMargins(0,0,0,0);
    auto center=new QWidget;auto centerLayout=new QVBoxLayout(center);centerLayout->setContentsMargins(0,0,0,0);
    pages->removeTab(pages->indexOf(scenePreview_));centerLayout->addWidget(scenePreview_);scenePreview_->show();
    // Keep inherited launch inputs available without consuming the scene canvas.
    // Child visibility still follows the selected scene's own requirements.
    auto setup=new QWidget(scenePreview_);setup->setObjectName("sceneOptionalSetup");
    auto setupLayout=new QVBoxLayout(setup);setupLayout->setContentsMargins(0,0,0,0);
    for(const auto name:{"sceneLaunchImageSetup","sceneLaunchContextSetup","sceneCastSetup","sceneNamesSetup","sceneRewardSetup"}){
        auto w=scenePreview_->findChild<QWidget *>(name);scenePreview_->layout()->removeWidget(w);setupLayout->addWidget(w);
    }
    static_cast<QVBoxLayout *>(scenePreview_->layout())->insertWidget(0,setup);
    auto sceneCanvas=scenePreview_->findChild<QGraphicsView *>("sceneCanvas");
    // Preserve a 640x480 image area, excluding the view's frame and fit margin.
    sceneCanvas->setMinimumWidth(648);sceneCanvas->setFixedHeight(488);
    sceneCanvas->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sceneCanvas->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scenePreview_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
    keyPicker_=new SceneKeyPicker(rom_);keyPicker_->setObjectName("cutscenePlacementLibrary");
    if(project_->combatArtwork())keyPicker_->setArtwork(project_->combatArtwork()->previewEdits());
    centerLayout->addWidget(keyPicker_->auditionWidget());keyPicker_->auditionWidget()->setFixedHeight(170);
    auto useKey=new QPushButton("Play key on selected object");useKey->setObjectName("sceneUseSelectedKey");
    static_cast<QVBoxLayout *>(keyPicker_->layout())->addWidget(useKey);
    connect(useKey,&QPushButton::clicked,this,[this,pages]{const auto key=keyPicker_->selectedKey();if(key.isEmpty())return;const auto object=scenePreview_->selectedObject();scenePreview_->clearAssetPlacement();actionEditor_->playObjectKey(object.kind==SceneObjectKind::Sprite,object.slot,key[1],key[2],key[3],scenePreview_->selectedObjectRecord(),key[4]);pages->setCurrentWidget(actionEditor_);});
    keyPicker_->setMinimumWidth(290);keyPicker_->setMaximumWidth(380);
    upperLayout->addWidget(center,1);upperLayout->addWidget(keyPicker_);
    bodyLayout->addWidget(upper);timeline_=new SceneTimeline;timeline_->setFixedHeight(240);bodyLayout->addWidget(timeline_);
    scroll->setWidget(body);layout->addWidget(scroll,1);
    pages->insertTab(0,workspace_,"Scene timeline");pages->setCurrentWidget(workspace_);
    pages->setTabText(pages->indexOf(actionEditor_),"Step details");
    for(int i=0;i<pages->count();++i)if(pages->tabText(i)=="Commands")pages->setTabText(i,"Raw commands");
    connect(keyPicker_,&SceneKeyPicker::keySelected,scenePreview_,&ScenePreview::armAssetPlacement);
    connect(keyPicker_,&SceneKeyPicker::placementCancelled,scenePreview_,&ScenePreview::clearAssetPlacement);
    connect(scenePreview_,&ScenePreview::assetPlaced,actionEditor_,&SceneActionEditor::addAsset);
    connect(scenePreview_,&ScenePreview::spritePlacedAtPlayhead,actionEditor_,&SceneActionEditor::placeSpriteAtPlayhead);
    connect(scenePreview_,&ScenePreview::timelineChanged,timeline_,&SceneTimeline::updatePlayback);
    connect(timeline_,&SceneTimeline::timeSelected,scenePreview_,&ScenePreview::seekToUpdate);
    connect(timeline_,&SceneTimeline::commandSelected,this,[this](int c){actionEditor_->selectCommand(c);timeline_->selectCommand(c);const auto &p=programs().program(selectedScene());if(c>=0&&c<p.commands.size()&&scenePreview_->commandTimes().contains(p.commands[c].word))scenePreview_->seekToCommand(p.commands[c].word);});
    connect(timeline_,&SceneTimeline::commandActivated,this,[this,pages](int c){actionEditor_->selectCommand(c);pages->setCurrentWidget(actionEditor_);});
    connect(actionEditor_,&SceneActionEditor::commandSelected,timeline_,&SceneTimeline::selectCommand);
    auto advanced=new QCheckBox("Raw / preview setup");advanced->setObjectName("cutsceneRawToggle");actionTools->addWidget(advanced);
    auto showPages=[=,this](bool show){setup->setVisible(show);mode_->setVisible(show);requests_->setVisible(show);images_->setVisible(show);sceneStatus_->setVisible(show);for(const auto name:{"scenePreviewContext","sceneImportPlaybackInputs","sceneClearPlaybackInputs"})if(auto w=scenePreview_->findChild<QWidget *>(name))w->setVisible(show);for(int i=0;i<pages->count();++i){const auto page=pages->widget(i);pages->setTabVisible(i,show||page==workspace_||page==actionEditor_||pages->tabText(i)=="Dialogue");}};
    connect(advanced,&QCheckBox::toggled,pages,showPages);showPages(false);
}
void CutsceneEditor::createSceneTrigger(){
    if(!selectedScene()||invalidSceneInput())return;
    try{
        int row=eventRows_->currentData().toInt();
        if(row<0){QStringList labels;for(int i=1;i<eventRows_->count();++i)labels<<eventRows_->itemText(i);
            bool accepted=false;const auto label=QInputDialog::getItem(this,"Cutscene trigger","Event row",labels,0,false,&accepted);
            if(!accepted)return;row=eventRows_->itemData(labels.indexOf(label)+1).toInt();}
        const auto id=selectedScene();const auto selectors=programs().resource(id).selectors;
        if(selectors.isEmpty())throw Error("This scene has no callable selector.");
        QJsonObject draft{{"name",programs().displayName(id)},{"group",row==1?0:1},{"target",-1},
            {"nodes",QJsonArray{QJsonObject{{"type","scene"},{"scene",selectors.first()}}}}};
        QJsonObject flags;
        if(auto result=editEventTree(project_,row,draft,this,&flags)){
            auto events=*project_->sceneEvents();events.apply(flags,programs());
            auto trees=events.flagEdits()[QString::number(row)].toObject()["trees"].toArray();trees<<*result;
            events.setTrees(row,trees,programs());
            if(undo_)undo_->push(new SceneTriggerCommand(project_,events.json()));else project_->setSceneEvents(events.json());
            if(changed_)changed_();refreshEventRows();eventRows_->setCurrentIndex(eventRows_->findData(row));reloadScenes(id);
        }
    }catch(const std::exception &e){QMessageBox::warning(this,"Cutscene trigger",QString::fromUtf8(e.what()));}
}
void CutsceneEditor::refreshEventRows(){
    if(!eventRows_||!project_||!project_->sceneEvents())return;
    const int selected=eventRows_->count()?eventRows_->currentData().toInt():-1;
    const QSignalBlocker block(eventRows_);eventRows_->clear();
    eventRows_->addItem("All event rows / unassigned scenes",-1);
    for(int row=0;row<SceneEvents::rowCapacity;++row){
        try{if(project_->sceneEvents()->program(row).isEmpty())continue;
            auto name=project_->sceneEvents()->missionName(row);if(name.isEmpty())name=EventScript::rowName(row);
            eventRows_->addItem(QString("%1 · %2").arg(row).arg(name),row);
        }catch(const std::exception &){}
    }
    eventRows_->setCurrentIndex(qMax(0,eventRows_->findData(selected)));
}
void CutsceneEditor::filterScenes(){
    if(!eventRows_||!project_||!project_->scenePrograms())return;
    const int row=eventRows_->currentData().toInt();QSet<int> selectors;
    actionEditor_->setProperty("eventRow",row);
    if(row>=0)try{
        for(const auto &event:EventFlow::read(rom_,*project_->sceneEvents(),*project_->scenePrograms(),row))
            for(const auto &action:event.actions)if(action.scene>=0)selectors<<action.scene;
        // Authored event trees carry their actions in the tree model; the flow
        // reader deliberately returns only their named entry, not duplicate steps.
        std::function<void(const QJsonValue &)> collect=[&](const QJsonValue &value){
            if(value.isArray()){for(const auto &child:value.toArray())collect(child);}
            else if(value.isObject()){const auto object=value.toObject();if(object["type"]=="scene")selectors<<object["scene"].toInt(-1);for(auto it=object.begin();it!=object.end();++it)if(it->isArray()||it->isObject())collect(*it);}
        };
        collect(project_->sceneEvents()->flagEdits()[QString::number(row)].toObject()["trees"]);
    }catch(const std::exception &e){sceneStatus_->setText(QString::fromUtf8(e.what()));return;}
    const auto search=findChild<QLineEdit *>("cutsceneScenesSearch");
    int first=-1;
    for(int i=0;i<scenes_->count();++i){
        auto item=scenes_->item(i);const auto uuid=item->data(Qt::UserRole+1).toString();
        const SceneId id=uuid.isEmpty()?SceneId(item->data(Qt::UserRole).toUInt()):SceneId::authored(uuid);
        bool matches=row<0;for(int s:programs().resource(id).selectors)matches|=selectors.contains(s);
        item->setHidden(!matches||(search&&!(item->text()+" "+item->toolTip()).contains(search->text(),Qt::CaseInsensitive)));
        if(!item->isHidden()&&first<0)first=i;
    }
    if(auto pages=findChild<QTabWidget *>("cutsceneDetailPages"))pages->setVisible(first>=0);
    if(auto empty=findChild<QLabel *>("cutsceneEmptySelection"))empty->setVisible(first<0);
    if(first<0){for(auto button:{duplicateScene_,renameScene_,removeScene_,wipeScene_,templateScene_})button->setEnabled(false);sceneStatus_->setText("No cutscenes match this event row and search.");scenes_->setCurrentRow(-1);scenePreview_->clearAssetPlacement();return;}
    if(!scenes_->currentItem()||scenes_->currentItem()->isHidden())scenes_->setCurrentRow(first);
}
}
