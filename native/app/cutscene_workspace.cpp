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
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
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
    workspace_ = new QWidget;
    workspace_->setObjectName("cutsceneWorkspace");
    auto layout = new QVBoxLayout(workspace_);
    layout->setContentsMargins(8, 6, 8, 6);
    auto heading = new QHBoxLayout;
    auto title = new QLabel;
    title->setObjectName("sceneStageTitle");
    title->setTextFormat(Qt::PlainText);
    title->setWordWrap(true);
    auto font = title->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    title->setFont(font);
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    heading->addWidget(title, 1);
    auto trigger = new QPushButton("Event trigger…");
    trigger->setObjectName("sceneCreateTrigger");
    heading->addWidget(trigger);
    connect(trigger, &QPushButton::clicked, this, &CutsceneEditor::createSceneTrigger);
    auto settings = new QPushButton("Playback details…");
    settings->setObjectName("sceneOpenSetup");
    heading->addWidget(settings);
    layout->addLayout(heading);

    auto split = new QSplitter(Qt::Vertical);
    split->setChildrenCollapsible(false);
    layout->addWidget(split, 1);
    pages->removeTab(pages->indexOf(scenePreview_));
    split->addWidget(scenePreview_);
    scenePreview_->show();
    auto previewLayout = static_cast<QVBoxLayout *>(scenePreview_->layout());
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(3);
    auto sceneCanvas = scenePreview_->findChild<QGraphicsView *>("sceneCanvas");
    sceneCanvas->setMinimumSize(200, 135);
    sceneCanvas->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sceneCanvas->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto details = new QDialog(scenePreview_);
    details->setWindowTitle("Playback details");
    details->resize(770, 540);
    auto detailLayout = new QVBoxLayout(details);
    auto detailScroll = new QScrollArea;
    detailScroll->setWidgetResizable(true);
    auto setup = new QWidget;
    setup->setObjectName("sceneOptionalSetup");
    auto setupLayout = new QVBoxLayout(setup);
    for (const auto name : {"scenePlaybackStatus", "sceneActorState", "scenePreviewContext",
                            "sceneLaunchImageSetup", "sceneLaunchContextSetup", "sceneCastSetup",
                            "sceneNamesSetup", "sceneRewardSetup", "scenePlaybackInputsStatus",
                            "sceneImportPlaybackInputs", "sceneClearPlaybackInputs"}) {
        auto widget = scenePreview_->findChild<QWidget *>(name);
        const bool hidden = widget->isHidden();
        setupLayout->addWidget(widget);
        widget->setVisible(!hidden);
    }
    auto rateRow = new QHBoxLayout;
    rateRow->addWidget(new QLabel("Playback speed"));
    rateRow->addWidget(scenePreview_->findChild<QSpinBox *>("scenePlaybackRate"));
    rateRow->addStretch();
    rateRow->addWidget(scenePreview_->findChild<QPushButton *>("sceneFitStage"));
    setupLayout->addLayout(rateRow);
    setupLayout->addStretch();
    detailScroll->setWidget(setup);
    detailLayout->addWidget(detailScroll);
    auto close = new QDialogButtonBox(QDialogButtonBox::Close);
    detailLayout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, details, &QDialog::reject);
    connect(settings, &QPushButton::clicked, details, &QDialog::open);

    createCastTools(previewLayout);

    auto sequence = new QWidget;
    auto sequenceLayout = new QVBoxLayout(sequence);
    sequenceLayout->setContentsMargins(0, 2, 0, 0);
    auto sequenceTools = new QHBoxLayout;
    auto label = new QLabel("Timeline");
    auto labelFont = label->font();
    labelFont.setBold(true);
    label->setFont(labelFont);
    sequenceTools->addWidget(label);
    sequenceTools->addStretch();
    auto edit = new QPushButton("Edit selected step");
    auto findAction = scenePreview_->findChild<QPushButton *>("sceneFindActorAction");
    setupLayout->insertWidget(2, findAction);
    connect(findAction, &QPushButton::clicked, details, &QDialog::accept);
    sequenceTools->addWidget(edit);
    connect(edit, &QPushButton::clicked, this, [this,pages] { pages->setCurrentWidget(actionEditor_); });
    auto compose = new QPushButton("Add step…");
    compose->setObjectName("sceneComposeStep");
    sequenceTools->addWidget(compose);
    sequenceLayout->addLayout(sequenceTools);
    auto composer = new QDialog(workspace_);
    composer->setWindowTitle("Add a scene step");
    auto fields = new QFormLayout(composer);
    auto action = new QComboBox;
    action->setObjectName("sceneWorkspaceAction");
    const auto kinds = SceneActions::kinds();
    for (auto [id, name] : kinds.asKeyValueRange()) action->addItem(name, id);
    auto where = new QComboBox;
    where->setObjectName("sceneWorkspaceInsertWhere");
    where->addItems({"At playhead", "Before selected step", "After selected step", "Choice: option 1", "Choice: option 2"});
    fields->addRow("Action", action);
    fields->addRow("Insert", where);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    buttons->addButton("Add step", QDialogButtonBox::AcceptRole)->setObjectName("sceneWorkspaceAdd");
    fields->addRow(buttons);
    connect(compose, &QPushButton::clicked, this, [=, this] { scenePreview_->pauseForEditing(); composer->open(); });
    connect(buttons, &QDialogButtonBox::rejected, composer, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, composer, &QDialog::accept);
    connect(composer, &QDialog::accepted, this, [=, this] {
        const auto id = selectedScene();
        const auto before = programs().program(id).source;
        actionEditor_->addFromWorkspace(action->currentData().toUInt(), where->currentIndex());
        if (programs().program(id).source != before) pages->setCurrentWidget(actionEditor_);
    });
    timeline_ = new SceneTimeline;
    sequenceLayout->addWidget(timeline_, 1);
    split->addWidget(sequence);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 1);
    split->setSizes({520, 185});
    pages->insertTab(0, workspace_, "Stage");
    pages->setCurrentWidget(workspace_);
    auto fitPage = [this, pages] {
        pages->setSizePolicy(pages->currentWidget() == workspace_ ? QSizePolicy::Ignored : QSizePolicy::Expanding, QSizePolicy::Expanding);
    };
    connect(pages, &QTabWidget::currentChanged, pages, fitPage);
    fitPage();
    pages->setTabText(pages->indexOf(actionEditor_), "Step details");
    for (int i = 0; i < pages->count(); ++i)
        if (pages->tabText(i) == "Commands") pages->setTabText(i, "Raw commands");
    connect(scenePreview_,&ScenePreview::timelineChanged,timeline_,&SceneTimeline::updatePlayback);
    connect(timeline_,&SceneTimeline::timeSelected,scenePreview_,&ScenePreview::seekToUpdate);
    connect(timeline_,&SceneTimeline::commandSelected,this,[this](int c){actionEditor_->selectCommand(c);timeline_->selectCommand(c);const auto &p=programs().program(selectedScene());if(c>=0&&c<p.commands.size()&&scenePreview_->commandTimes().contains(p.commands[c].word))scenePreview_->seekToCommand(p.commands[c].word);});
    connect(timeline_,&SceneTimeline::commandActivated,this,[this,pages](int c){actionEditor_->selectCommand(c);pages->setCurrentWidget(actionEditor_);});
    connect(actionEditor_,&SceneActionEditor::commandSelected,timeline_,&SceneTimeline::selectCommand);
    auto advanced = new QCheckBox("Show technical tabs");
    advanced->setObjectName("cutsceneRawToggle");
    detailLayout->insertWidget(1, advanced);
    auto showPages = [=, this](bool show) {
        for (auto widget : {mode_, requests_, images_}) widget->setVisible(show);
        sceneStatus_->setVisible(show);
        for (int i = 0; i < pages->count(); ++i)
            pages->setTabVisible(i, show || pages->widget(i) == workspace_ || pages->widget(i) == actionEditor_ || pages->tabText(i) == "Dialogue" || pages->tabText(i) == "Background preview");
    };
    connect(advanced, &QCheckBox::toggled, pages, showPages);
    showPages(false);
}
void CutsceneEditor::createCastTools(QVBoxLayout *previewLayout) {
    auto tools = new QHBoxLayout;
    auto addCast = new QPushButton("Add actor or sprite…");
    addCast->setObjectName("sceneAddCast");
    tools->addWidget(addCast);
    auto cast = scenePreview_->findChild<QComboBox *>("sceneLiveActors");
    tools->addWidget(new QLabel("Cast"));
    cast->setMaximumWidth(210);
    tools->addWidget(cast);
    auto reposition = new QPushButton("Reposition");
    reposition->setObjectName("sceneReposition");
    auto move = new QPushButton("Move…");
    move->setObjectName("sceneMove");
    auto animate = new QPushButton("Animation…");
    animate->setObjectName("sceneAnimate");
    for (auto button : {reposition, move, animate}) {
        tools->addWidget(button);
        button->setEnabled(false);
        connect(scenePreview_, &ScenePreview::castEditable, button, &QPushButton::setEnabled);
    }
    tools->addStretch();
    reposition->setToolTip("Click a point on stage to place the selected cast member there immediately.");
    move->setToolTip("Choose a duration, then click a destination on stage.");
    previewLayout->insertLayout(0, tools);

    createArtworkDialog(addCast, animate, cast);
    auto motion = new QDialog(scenePreview_);
    motion->setObjectName("sceneMotionDialog");
    auto motionLayout = new QFormLayout(motion);
    auto duration = new QSpinBox;
    duration->setObjectName("sceneMoveDuration");
    duration->setRange(1, 1800);
    duration->setValue(30);
    duration->setSuffix(" updates");
    motionLayout->addRow("Travel time", duration);
    auto wait = new QCheckBox("Wait for arrival before the next step");
    wait->setObjectName("sceneMoveWait");
    wait->setChecked(true);
    motionLayout->addRow(wait);
    auto motionButtons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    motionButtons->addButton("Choose destination", QDialogButtonBox::AcceptRole)->setObjectName("sceneChooseDestination");
    motionLayout->addRow(motionButtons);
    connect(motionButtons, &QDialogButtonBox::rejected, motion, &QDialog::reject);
    connect(motionButtons, &QDialogButtonBox::accepted, motion, &QDialog::accept);
    connect(move, &QPushButton::clicked, this, [=, this] {
        scenePreview_->pauseForEditing();
        motion->setWindowTitle(tr("Move %1").arg(cast->currentText()));
        motion->open();
    });
    connect(motion, &QDialog::accepted, this, [=, this] {
        scenePreview_->beginCastPlacement(true);
    });
    connect(reposition, &QPushButton::clicked, this, [this] { scenePreview_->beginCastPlacement(false); });
    connect(scenePreview_, &ScenePreview::castDestination, this, [=, this](bool sprite, int slot, SceneVector position, bool moving) {
        actionEditor_->setPlacementMode(moving, duration->value(), wait->isChecked());
        if (sprite) actionEditor_->placeSpriteAtPlayhead(slot, position);
        else actionEditor_->placeActorAtPlayhead(slot, position);
        actionEditor_->setPlacementMode(false, duration->value(), false);
    });
    connect(scenePreview_, &ScenePreview::assetPlaced, actionEditor_, &SceneActionEditor::addAsset);
    connect(scenePreview_, &ScenePreview::spritePlacedAtPlayhead, actionEditor_, &SceneActionEditor::placeSpriteAtPlayhead);
    connect(actionEditor_, &SceneActionEditor::stageFeedback, scenePreview_, &ScenePreview::showEditFeedback);
    connect(actionEditor_, &SceneActionEditor::assetAdded, scenePreview_, &ScenePreview::selectCastMember);
}
void CutsceneEditor::createArtworkDialog(QPushButton *addCast, QPushButton *animate, QComboBox *cast) {
    auto library = new QDialog(scenePreview_);
    library->setObjectName("sceneArtworkDialog");
    library->resize(690, 540);
    auto libraryLayout = new QVBoxLayout(library);
    keyPicker_ = new SceneKeyPicker(rom_);
    keyPicker_->setObjectName("cutscenePlacementLibrary");
    if (project_->combatArtwork()) keyPicker_->setArtwork(project_->combatArtwork()->previewEdits());
    libraryLayout->addWidget(keyPicker_, 1);
    libraryLayout->addWidget(keyPicker_->auditionWidget());
    auto libraryButtons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto useKey = libraryButtons->addButton("Place on stage", QDialogButtonBox::AcceptRole);
    useKey->setObjectName("sceneUseSelectedKey");
    libraryLayout->addWidget(libraryButtons);
    auto grid = keyPicker_->findChild<QListWidget *>("sceneKeyGrid");
    auto kind = keyPicker_->findChild<QComboBox *>("sceneKeyKind");
    auto allowKey = [=] {
        const auto item = grid->currentItem();
        useKey->setEnabled(item != nullptr);
        const bool animate = library->property("changingAnimation").toBool();
        const bool empty = item && !item->data(Qt::UserRole + 5).toBool();
        useKey->setText(animate ? (empty ? "Apply empty animation" : "Apply animation") :
                                  (empty ? "Place empty artwork" : "Place on stage"));
    };
    connect(keyPicker_, &SceneKeyPicker::keySelected, library, allowKey);
    connect(keyPicker_, &SceneKeyPicker::placementCancelled, library, [useKey] { useKey->setEnabled(false); });
    auto openLibrary = [=, this](bool changingAnimation) {
        scenePreview_->pauseForEditing();
        library->setProperty("changingAnimation", changingAnimation);
        library->setWindowTitle(changingAnimation ? tr("Animation for %1").arg(cast->currentText()) : tr("Add an actor or sprite"));
        useKey->setText(changingAnimation ? "Apply animation" : "Place on stage");
        if (changingAnimation) kind->setCurrentIndex(scenePreview_->selectedObject().kind == SceneObjectKind::Sprite ? 1 : 0);
        kind->setEnabled(!changingAnimation);
        if (changingAnimation) {
            const auto record = scenePreview_->selectedObjectRecord();
            const bool sprite = kind->currentIndex() == 1;
            if (!record.isEmpty()) keyPicker_->selectArtwork(
                int(u32(record,sprite ? 0x30 : 0xe8)), u8(record,sprite ? 0x4c : 0x146),
                qint16(u16(record,sprite ? 0x44 : 0x138)), u8(record,sprite ? 0x4b : 0x13f));
        }
        keyPicker_->openLibrary(!changingAnimation);
        allowKey();
        library->open();
    };
    connect(addCast, &QPushButton::clicked, this, [openLibrary] { openLibrary(false); });
    connect(animate, &QPushButton::clicked, this, [openLibrary] { openLibrary(true); });
    connect(libraryButtons, &QDialogButtonBox::rejected, library, &QDialog::reject);
    connect(libraryButtons, &QDialogButtonBox::accepted, library, &QDialog::accept);
    connect(library, &QDialog::accepted, this, [=, this] {
        const auto key = keyPicker_->selectedKey();
        if (key.isEmpty()) return;
        if (!library->property("changingAnimation").toBool()) {
            scenePreview_->armAssetPlacement(key[0] == 1, key[1], key[2], key[3], key[4]);
            return;
        }
        const auto object = scenePreview_->selectedObject();
        actionEditor_->playObjectKey(key[0] == 1, object.slot, key[1], key[2], key[3],
                                     scenePreview_->selectedObjectRecord(), key[4]);
    });
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
