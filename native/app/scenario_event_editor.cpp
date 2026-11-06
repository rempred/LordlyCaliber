#include "app/scenario_event_editor.h"
#include "app/event_flow_editor.h"
#include "app/scenario_flowchart.h"
#include "app/pending_edits.h"
#include <QScopedValueRollback>
#include "app/content_refresh.h"
#include "engine/event_script.h"
#include "engine/event_authoring.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class EventTextDelegate final : public QStyledItemDelegate {
  public:
    explicit EventTextDelegate(QTreeWidget *tree) : QStyledItemDelegate(tree), tree_(tree) {}
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        if (index.column() == 1) {
            auto prepared = option; initStyleOption(&prepared, index);
            const int width = qMax(40, tree_->columnWidth(1) - 12);
            const auto text = prepared.fontMetrics.boundingRect(QRect(0, 0, width, 0),
                Qt::TextWordWrap, prepared.text);
            size.setHeight(qMax(size.height(), text.height() + 6));
        }
        return size;
    }
  private:
    QTreeWidget *tree_;
};
class FlagCommand final : public QUndoCommand {
  public:
    FlagCommand(Project *p, QJsonObject after, const QString &label)
        : QUndoCommand(label), p_(p), before_(p->sceneEvents()->json()), after_(after) {}
    void undo() override { p_->setSceneEvents(before_); }
    void redo() override { p_->setSceneEvents(after_); }
  private:
    Project *p_;
    QJsonObject before_, after_;
};
constexpr int offsetRole = Qt::UserRole, flagRole = Qt::UserRole + 1,
    checkRole = Qt::UserRole + 2, valueRole = Qt::UserRole + 3, targetRole = Qt::UserRole + 4;
QString position(int at) { return QString("@%1").arg(at, 4, 16, QChar('0')).toUpper(); }
}
ScenarioEventEditor::ScenarioEventEditor(Project *p, QUndoStack *undo, std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(p), undo_(undo), changed_(std::move(changed)) {
    setObjectName("scenarioEvents");
    auto outer=new QVBoxLayout(this);
    auto mode=new QHBoxLayout;viewChoice_=new QTabBar;viewChoice_->setObjectName("scenarioEventViewChoice");
    viewChoice_->setExpanding(false);viewChoice_->addTab("Event tree");viewChoice_->addTab("Scenario flowchart");mode->addWidget(viewChoice_);mode->addStretch();
    otherPrograms_=new QCheckBox("All event programs");otherPrograms_->setObjectName("scenarioEventOtherPrograms");mode->addWidget(otherPrograms_);
    raw_=new QCheckBox("Raw instructions");raw_->setObjectName("scenarioEventRaw");mode->addWidget(raw_);outer->addLayout(mode);
    auto locationLine=new QHBoxLayout;
    locationLabel_=new QLabel("Map location");locationLine->addWidget(locationLabel_);
    locations_=new QComboBox;locations_->setObjectName("scenarioEventLocation");locationLine->addWidget(locations_,1);outer->addLayout(locationLine);
    createScenario_=new QPushButton("Create scenario…");createScenario_->setObjectName("storyCreateScenario");locationLine->addWidget(createScenario_);createScenario_->hide();
    connect(createScenario_,&QPushButton::clicked,this,[this]{if(createScenarioCallback_&&location_>0)createScenarioCallback_(location_);});
    phases_=new QTabBar;phases_->setObjectName("scenarioEventPhase");phases_->setExpanding(false);
    phases_->addTab("Arrival");phases_->addTab("Mission");phases_->addTab("Area Investigation");outer->addWidget(phases_);
    rows_ = new QComboBox; rows_->setObjectName("scenarioEventRow");outer->addWidget(rows_);
    views_=new QStackedWidget;outer->addWidget(views_,1);
    eventsView_=new EventFlowEditor(project_,undo_,[this]{refresh();changed_();},this);views_->addWidget(eventsView_);
    eventsView_->rawInstructions=[this](int at){setRawInstructions(true);selectEventRow(rows_->currentData().toInt(),at);};
    eventsView_->editCutscene=[this](int selector){if(editCutscene)editCutscene(selector);};
    eventsView_->openEventProgram=[this](int row){selectEventRow(row);};
    eventsView_->openEventInstruction=[this](int row,int at){setRawInstructions(false);selectEventRow(row,at);};
    auto rawPage=new QWidget;views_->addWidget(rawPage);auto layout = new QVBoxLayout(rawPage);layout->setContentsMargins(0,0,0,0);
    chart_=new ScenarioFlowchart(project_,this);views_->addWidget(chart_);
    chart_->navigate=[this](ChartAddress address){
        viewChoice_->setCurrentIndex(0);
        selectEventRow(address.row,address.offset);
        if(!address.tree.isEmpty())eventsView_->editTree(address.tree,address.path,address.offset);
    };
    chart_->editStructure=[this](ChartAddress address,ChartTreeEdit operation){
        try{
            auto next=*project_->sceneEvents();const auto &scenes=*project_->scenePrograms();
            auto trees=next.flagEdits()[QString::number(address.row)].toObject()["trees"].toArray();
            int index=-1;for(int i=0;i<trees.size();++i)if(trees[i].toObject()["id"]==address.tree){index=i;break;}
            if(index<0&&!address.tree.isEmpty())throw Error("This event was removed");
            if(operation==ChartTreeEdit::Remove&&address.path.isEmpty()&&index>=0){
                if(QMessageBox::question(chart_,"Delete event","Delete "+trees[index].toObject()["name"].toString()+"?",
                   QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
                trees.removeAt(index);next.setTrees(address.row,trees,scenes);commit(next,"Delete scenario event");refresh();return;
            }
            QJsonObject flags;
            auto result=editEventTree(project_,address.row,index>=0?trees[index].toObject():QJsonObject{},chart_,&flags,address.path,operation,true);
            if(!result)return;
            next.apply(flags,scenes);if(index>=0)trees[index]=*result;else trees.append(*result);
            next.setTrees(address.row,trees,scenes);commit(next,"Edit scenario flowchart");refresh();
            chart_->focusTree(address.row,(*result)["id"].toString(),operation==ChartTreeEdit::Remove?QVector<int>{}:address.path);
        }catch(const std::exception &e){QMessageBox::warning(chart_,"Edit event",QString::fromUtf8(e.what()));}
    };
    chart_->edit=[this](ChartAddress address){
        if(!address.tree.isEmpty())chart_->editStructure(address,ChartTreeEdit::Edit);
        else if(address.gateControl&&chart_->editGate)chart_->editGate(address,-1,false);
        else {eventsView_->editChartStep(address.row,address.offset,chart_);refresh();}
    };
    chart_->editGate=[this](ChartAddress address,int group,bool remove){
        if(address.gate<0)return;
        try{
            auto next=*project_->sceneEvents();auto flags=next.json();
            auto old=next.flagEdits()[QString::number(address.row)].toObject()["gates"].toObject()[QString::number(address.gate)].toObject();
            auto friendly=[&](QJsonObject value){
                if(address.gateReversed)for(const QString key:{"yes","no","outcome"}){
                    if(value[key]=="yes")value[key]="no";else if(value[key]=="no")value[key]="yes";
                }return value;
            };
            QJsonObject gate;
            if(remove){
                if(address.gateControl&&old["mode"]=="if"){
                    // Removing an added check restores the complete original decision.
                    gate={};
                }else{
                    QMessageBox dialog(QMessageBox::Question,"Remove condition","Which branch should run in its place?",QMessageBox::Cancel,chart_);
                    auto yes=dialog.addButton("Keep Yes",QMessageBox::ActionRole),no=dialog.addButton("Keep No",QMessageBox::ActionRole);
                    dialog.exec();if(dialog.clickedButton()!=yes&&dialog.clickedButton()!=no)return;
                    gate=friendly({{"mode","keep"},{"outcome",dialog.clickedButton()==yes?"yes":"no"}});
                    if(old["mode"]=="if"){
                        const auto outcome=gate["outcome"].toString();
                        gate=old;
                        for(const QString key:{"yes","no"})if(gate[key]=="original")gate[key]=outcome;
                    }
                }
            }else if(address.gateControl&&old["mode"]=="keep"){
                if(QMessageBox::question(chart_,"Restore condition","Restore the original IF condition?",
                    QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
            }else{
                if(group<0){
                    const auto events=EventFlow::read(project_->rom(),next,*project_->scenePrograms(),address.row);
                    for(const auto &event:events){
                        if(event.triggerInstructions.contains(address.gate))group=event.group;
                        for(const auto &step:event.requirements+event.actions)if(step.instructions.contains(address.gate))group=event.group;
                    }
                }
                auto result=editRetailCondition(project_,address.row,group,friendly(old),chart_,&flags);
                if(!result)return;gate=friendly(*result);next.apply(flags,*project_->scenePrograms());
            }
            next.setRetailGate(address.row,address.gate,gate,*project_->scenePrograms());
            commit(next,"Edit event condition");refresh();
        }catch(const std::exception &e){QMessageBox::warning(chart_,"Edit condition",QString::fromUtf8(e.what()));}
    };
    chart_->createEvent=[this](int row){
        if(!EventAuthoring::supported(row)){
            QMap<QString,int> programs;
            if(otherPrograms_->isChecked()&&rows_->currentIndex()>=0){const int id=rows_->currentData().toInt();if(EventAuthoring::supported(id))programs[EventScript::rowName(id)]=id;}
            else if(location_>0){
                const auto context=contexts_->location(location_);
                for(int phase:{1,2})for(const auto &p:context.phases[phase])if(EventAuthoring::supported(p.row))
                    programs[phases_->tabText(phase)+" · "+p.name+" ["+QString::number(p.row)+"]"]=p.row;
            }else for(int id:contexts_->scenarioPrograms(scenario_))if(EventAuthoring::supported(id))programs[EventScript::rowName(id)]=id;
            if(programs.isEmpty())return;
            row=programs.first();
            if(programs.size()>1){bool ok=false;const auto choice=QInputDialog::getItem(chart_,"Create event","Add to",programs.keys(),0,false,&ok);if(!ok)return;row=programs[choice];}
        }
        chart_->editStructure({row},ChartTreeEdit::Edit);
    };
    connect(viewChoice_,&QTabBar::currentChanged,this,[this]{updateView();});
    connect(raw_,&QCheckBox::toggled,this,[this](bool checked){
        if(!checked&&!drafts_.isEmpty()&&!applyDrafts()){const QSignalBlocker guard(raw_);raw_->setChecked(true);return;}
        views_->setCurrentIndex(checked?1:0);
        viewChoice_->setEnabled(!checked);
        if(checked&&eventsView_->selectedOffset()>=0&&rows_->currentIndex()>=0)selectEventRow(rows_->currentData().toInt(),eventsView_->selectedOffset());
        if(!checked&&rows_->currentIndex()>=0)eventsView_->setRow(rows_->currentData().toInt(),selectedEntry());
        if(!checked&&steps_->currentItem())eventsView_->selectOffset(steps_->currentItem()->data(0,offsetRole).toInt());});
    auto help = new QLabel("ROM instructions: CHECK tests a flag; SET / CLEAR changes it when this step runs.");
    help->setWordWrap(true); layout->addWidget(help);
    all_ = new QCheckBox("Show all instructions");all_->setChecked(true);
    all_->setObjectName("scenarioEventAll"); layout->addWidget(all_);
    steps_ = new QTreeWidget; steps_->setObjectName("scenarioEventSteps");
    steps_->setHeaderLabels({"Position", "Event / action", "Editable values"});
    steps_->header()->setStretchLastSection(false);
    steps_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    steps_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    steps_->header()->setSectionResizeMode(2,QHeaderView::Interactive);steps_->setColumnWidth(2,380);
    steps_->setWordWrap(true); steps_->setTextElideMode(Qt::ElideNone);
    steps_->setItemDelegate(new EventTextDelegate(steps_));
    connect(steps_->header(), &QHeaderView::sectionResized, steps_, &QTreeWidget::doItemsLayout);
    layout->addWidget(steps_, 1);
    detail_ = new QLabel; detail_->setWordWrap(true); detail_->setObjectName("scenarioEventDetail");
    layout->addWidget(detail_);
    flagPanel_=new QWidget;auto form = new QFormLayout(flagPanel_);form->setContentsMargins(0,0,0,0);
    flags_ = new QComboBox; flags_->setObjectName("scenarioEventFlag");
    flags_->setEditable(true); flags_->setInsertPolicy(QComboBox::NoInsert);
    flags_->completer()->setCompletionMode(QCompleter::PopupCompletion);
    flags_->completer()->setFilterMode(Qt::MatchContains);
    flags_->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    for (int flag = 0; flag < 256; ++flag) flags_->addItem(EventScript::flagName(flag), flag);
    action_ = new QComboBox; action_->setObjectName("scenarioEventAction");
    form->addRow("Flag", flags_); form->addRow("Action", action_); layout->addWidget(flagPanel_);
    branchPanel_=new QWidget;auto branch = new QHBoxLayout(branchPanel_);branch->setContentsMargins(0,0,0,0);
    branchTarget_ = new QComboBox; branchTarget_->setObjectName("scenarioEventBranchTarget");
    branchTarget_->setMinimumContentsLength(35);
    branchTarget_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    branch->addWidget(new QLabel("Branch destination")); branch->addWidget(branchTarget_,1);
    applyBranch_ = new QPushButton("Change destination"); applyBranch_->setObjectName("scenarioEventApplyBranch");
    branch->addWidget(applyBranch_);layout->addWidget(branchPanel_);
    connect(applyBranch_, &QPushButton::clicked, this, [this] {
        const auto item=steps_->currentItem();if(!item || !item->parent() || branchTarget_->currentIndex()<0)return;
        try {
            auto next=*project_->sceneEvents();
            next.editBranch(rows_->currentData().toInt(),item->data(0,offsetRole).toInt(),branchTarget_->currentData().toInt());
            commit(next,"Change story branch destination");
        } catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
    });
    auto buttons = new QHBoxLayout;
    apply_ = new QPushButton("Apply step"); apply_->setObjectName("scenarioEventApply"); buttons->addWidget(apply_);
    discard_ = new QPushButton("Discard pending step"); discard_->setObjectName("scenarioEventDiscard"); buttons->addWidget(discard_);
    jump_ = new QPushButton("Go to branch destination"); jump_->setObjectName("scenarioEventJump"); buttons->addWidget(jump_);
    openCutscene_=new QPushButton("Edit this cutscene…");openCutscene_->setObjectName("scenarioEventOpenCutscene");buttons->addWidget(openCutscene_);
    connect(openCutscene_,&QPushButton::clicked,this,[this]{
        const auto item=steps_->currentItem();if(!item||!item->parent()||!editCutscene)return;
        for(const auto &site:SceneEvents::sites())if(site.row==rows_->currentData().toInt()&&site.offset==item->data(0,offsetRole).toInt()) {editCutscene(project_->sceneEvents()->target(site.id));return;}
    });
    buttons->addStretch();
    restore_ = new QPushButton("Restore loaded instruction values"); restore_->setObjectName("scenarioEventRestore"); buttons->addWidget(restore_);
    layout->addLayout(buttons);
    notice_ = new QLabel; notice_->setWordWrap(true); notice_->setObjectName("scenarioEventNotice"); layout->addWidget(notice_);
    pending_ = new PendingEdits(this);
    pending_->entries = [this] {
        QVector<PendingEdit> entries;
        for (auto it = drafts_.cbegin(); it != drafts_.cend(); ++it)
            entries.append({QString("Event row %1 · flag instruction %2").arg(it.key().first).arg(position(it.key().second)),
                            {allRows_ ? "story-event" : "scenario-event", it.key().first, it.key().second}});
        return entries;
    };
    pending_->apply = [this] { return applyDrafts(); };
    pending_->discard = [this] { drafts_.clear(); selectStep(); pending_->notify(); };
    connect(discard_, &QPushButton::clicked, this, [this] { drafts_.remove(selection_); selectStep(); pending_->notify(); });
    connect(flags_, &QComboBox::currentTextChanged, this, [this] { rememberDraft(); });
    connect(action_, &QComboBox::currentIndexChanged, this, [this] { rememberDraft(); });
    connect(rows_, &QComboBox::currentIndexChanged, this, [this] { if (!loading_) showRow(); });
    connect(otherPrograms_,&QCheckBox::toggled,this,[this]{if(!loading_)refresh();});
    connect(locations_,&QComboBox::currentIndexChanged,this,[this]{
        if(loading_)return;location_=locations_->currentData().toInt();refresh();});
    connect(phases_,&QTabBar::currentChanged,this,[this]{if(!loading_)refresh();});
    connect(all_, &QCheckBox::toggled, this, [this] { showRow(); });
    connect(steps_, &QTreeWidget::currentItemChanged, this, [this] { selectStep(); });
    connect(apply_, &QPushButton::clicked, this, &ScenarioEventEditor::apply);
    connect(jump_, &QPushButton::clicked, this, [this] {
        const auto item = steps_->currentItem(); if (!item) return;
        const int target = item->data(0, targetRole).toInt();
        all_->setChecked(true);
        for (int h = 0; h < steps_->topLevelItemCount(); ++h) {
            auto parent = steps_->topLevelItem(h);
            for (int n = 0; n < parent->childCount(); ++n)
                if (parent->child(n)->data(0, offsetRole).toInt() == target) {
                    parent->setExpanded(true); steps_->setCurrentItem(parent->child(n));
                    steps_->scrollToItem(parent->child(n)); return;
                }
        }
        notice_->setText("This branch destination is outside the decoded portion of this handler.");
    });
    connect(restore_, &QPushButton::clicked, this, [this] {
        if (rows_->currentIndex() < 0) return;
        auto next = *project_->sceneEvents(); next.restoreFlags(rows_->currentData().toInt());
        for (auto it = drafts_.begin(); it != drafts_.end(); )
            if (it.key().first == rows_->currentData().toInt()) it = drafts_.erase(it); else ++it;
        commit(next, "Restore scenario event flags");
    });
    watchContentChanges(undo_, this, [this] { chartDirty_=true;if (isVisible()) refresh(); });
    selectStep();
}
void ScenarioEventEditor::selectScenario(int key) {
    const bool changed = key != scenario_; scenario_ = key;
    if (changed) { QSignalBlocker block(rows_); rows_->clear();
        const QSignalBlocker programsBlock(otherPrograms_);otherPrograms_->setChecked(false);
        const QSignalBlocker phaseBlock(phases_);phases_->setCurrentIndex(1); }
    if (changed && isVisible()) refresh();
}
void ScenarioEventEditor::selectLocation(int location) {
    if(location<1||location>40)return;
    const bool changed=location_!=location||!allRows_;location_=location;allRows_=true;
    if(changed){const QSignalBlocker block(otherPrograms_);otherPrograms_->setChecked(false);}
    if(changed||!contexts_)refresh();
}
void ScenarioEventEditor::showAllRows() { allRows_=true;all_->setChecked(true);refresh(); }
void ScenarioEventEditor::setRawInstructions(bool enabled){if(enabled)viewChoice_->setCurrentIndex(0);raw_->setChecked(enabled);}
void ScenarioEventEditor::selectEventRow(int row, int offset) {
    if(offset>=0)viewChoice_->setCurrentIndex(0);
    if(!contexts_)refresh();
    try {
    if(!otherPrograms_->isChecked()&&contexts_&&allRows_){
        const auto locations=contexts_->locationsForProgram(row,offset);
        if(!locations.isEmpty()&&!locations.contains(location_)){location_=locations.first();refresh();}
    }
    if(!otherPrograms_->isChecked()&&contexts_&&location_>0&&rows_->findData(row)<0){
        const auto context=contexts_->location(location_);
        for(int phase=0;phase<3;++phase)for(const auto &choice:context.phases[phase])if(choice.row==row){
            const QSignalBlocker block(phases_);phases_->setCurrentIndex(phase);populatePrograms(row);break;
        }
    }
    }catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));}
    if(rows_->findData(row)<0){otherPrograms_->setChecked(true);if(rows_->findData(row)<0)refresh();}
    {const QSignalBlocker block(rows_);rows_->setCurrentIndex(rows_->findData(row));}showRow();
    if(offset>=0&&!raw_->isChecked())eventsView_->selectOffset(offset);
    if(offset>=0)for(int i=0;i<steps_->topLevelItemCount();++i) {
        auto handler=steps_->topLevelItem(i);
        for(int j=0;j<handler->childCount();++j)if(handler->child(j)->data(0,offsetRole).toInt()==offset) {
            steps_->setCurrentItem(handler->child(j));steps_->scrollToItem(handler->child(j));return;
        }
    }
}
void ScenarioEventEditor::showEvent(QShowEvent *event) { QWidget::showEvent(event); refresh(); }
void ScenarioEventEditor::refresh() {
    loading_ = true;
    const int previous = rows_->currentIndex() >= 0 ? rows_->currentData().toInt() : -1;
    rows_->clear(); notice_->clear();
    try {
        project_->loadSceneEvents();project_->loadCampaignMap();
        refreshContext();populatePrograms(previous);
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
    loading_ = false;
    showRow();
}
void ScenarioEventEditor::refreshContext(){
    const auto signature=QJsonDocument(project_->sceneEvents()->json()).toJson(QJsonDocument::Compact);
    const auto rom=project_->rom().bytes().constData();
    if(!contexts_||signature!=contextSignature_||rom!=contextRom_){
        chartDirty_=true;
        contexts_=std::make_unique<EventContexts>(project_->rom(),*project_->sceneEvents());
        contextSignature_=signature;contextRom_=rom;
    }
    if(otherPrograms_->isChecked()){
        locations_->hide();locationLabel_->hide();phases_->hide();return;
    }
    const QSignalBlocker locationBlock(locations_);locations_->clear();
    QVector<int> available;
    if(allRows_)for(int id=1;id<=40;++id){if(project_->campaignMap()->position(id).x())available<<id;}
    else available=contexts_->locationsForScenario(scenario_);
    for(int id:available)locations_->addItem(project_->campaignMap()->name(id),id);
    if(!available.contains(location_))location_=available.isEmpty()?-1:available.first();
    locations_->setCurrentIndex(locations_->findData(location_));
    const bool showPicker=!otherPrograms_->isChecked()&&(allRows_||locations_->count()>1);
    locations_->setVisible(showPicker);
    locationLabel_->setVisible(!otherPrograms_->isChecked());
    locationLabel_->setText(showPicker?"Map location":location_>0?project_->campaignMap()->name(location_):"Selected scenario");
    phases_->setVisible(!otherPrograms_->isChecked());
    // Keep empty phases selectable so the reason is clear instead of silently
    // switching the user's activity when a location has no corresponding event.
}
int ScenarioEventEditor::selectedEntry()const{
    return rows_->currentIndex()<0?-1:rows_->currentData(Qt::UserRole+1).toInt();
}
void ScenarioEventEditor::populatePrograms(int preferredRow){
    const QSignalBlocker block(rows_);rows_->clear();
    const bool all=otherPrograms_->isChecked();
    if(all){
        for(int row=0;row<SceneEvents::rowCapacity;++row){const auto name=project_->sceneEvents()->missionName(row);
            if(name.isEmpty()&&EventScript::instructionOffsets(row).isEmpty())continue;
            rows_->addItem(name.isEmpty()?EventScript::rowName(row):name,row);rows_->setItemData(rows_->count()-1,-1,Qt::UserRole+1);}
    }else if(contexts_){
        QVector<EventProgramChoice> choices;
        if(location_>0)choices=contexts_->location(location_).phases[qMax(0,phases_->currentIndex())];
        else if(scenario_>=0&&phases_->currentIndex()>0)for(int row:contexts_->scenarioPrograms(scenario_)){
            const bool investigation=row>=71&&row<=109;
            if(investigation==(phases_->currentIndex()==2))choices.append({row,-1,EventContexts::programName(row)});
        }
        for(const auto &choice:choices){rows_->addItem(choice.name,choice.row);rows_->setItemData(rows_->count()-1,choice.entry,Qt::UserRole+1);}
    }
    rows_->setCurrentIndex(qMax(0,rows_->findData(preferredRow)));
    rows_->setVisible(all||rows_->count()>1);
}
void ScenarioEventEditor::setCreateScenario(std::function<void(int)> callback){
    createScenarioCallback_=std::move(callback);updateView();
}
void ScenarioEventEditor::updateView(){
    createScenario_->setVisible(bool(createScenarioCallback_)&&!otherPrograms_->isChecked()&&location_>0);
    const bool chart=viewChoice_->currentIndex()==1;
    raw_->setVisible(!chart);
    phases_->setVisible(!chart&&!otherPrograms_->isChecked());
    rows_->setVisible((!chart||otherPrograms_->isChecked())&&(otherPrograms_->isChecked()||rows_->count()>1));
    views_->setCurrentIndex(chart?2:raw_->isChecked()?1:0);
    if(chart)showChart();
    else if(!loading_)showRow();
}
void ScenarioEventEditor::showChart(){
    if(!project_->sceneEvents())return;
    const bool single=otherPrograms_->isChecked();
    const int row=rows_->currentIndex()>=0?rows_->currentData().toInt():-1;
    const QString key=single?QString("program/%1").arg(row):QString("scenario/%1/location/%2").arg(scenario_).arg(location_);
    if(key==chartKey_&&!chartDirty_)return;
    QVector<QPair<QString,EventProgramChoice>> choices;
    try{
        if(single&&row>=0)choices.append({"Event program",{row,-1,EventContexts::programName(row)}});
        else if(contexts_&&location_>0){
            const auto context=contexts_->location(location_);
            for(int phase=0;phase<3;++phase)for(const auto &p:context.phases[phase])
                choices.append({phases_->tabText(phase),p});
        }else if(contexts_&&scenario_>=0)for(int id:contexts_->scenarioPrograms(scenario_))
            choices.append({id>=71&&id<=109?"Area Investigation":"Mission",{id,-1,EventContexts::programName(id)}});
        chart_->setScenario(key,scenarioChart(*project_,choices));chartKey_=key;chartDirty_=false;
    }catch(const std::exception &e){chart_->setScenario(key,{});QMessageBox::warning(this,"Scenario flowchart",QString::fromUtf8(e.what()));}
}
void ScenarioEventEditor::showRow() {
    if(viewChoice_->currentIndex()==1){updateView();return;}
    const QScopedValueRollback<bool> loading(loading_,true);
    const int previous = steps_->currentItem() ? steps_->currentItem()->data(0, offsetRole).toInt() : -1;
    const QSignalBlocker block(steps_);
    clearParameters();steps_->clear();
    restore_->setEnabled(rows_->currentIndex() >= 0);
    if (rows_->currentIndex() < 0) { eventsView_->setRow(-1);
        const auto message=QString("No %1 events are assigned to this map location.").arg(phases_->tabText(phases_->currentIndex()));
        if(notice_->text().isEmpty())notice_->setText(message);
        detail_->setText(notice_->text());selectStep();return; }
    try {
        const int row = rows_->currentData().toInt();
        if(!raw_->isChecked())eventsView_->setRow(row,selectedEntry());
        {
            const QSignalBlocker flagBlock(flags_);
            for (int i = 0; i < flags_->count(); ++i)
                flags_->setItemText(i, EventScript::flagName(flags_->itemData(i).toInt(), row));
        }
        const auto bytes = project_->sceneEvents()->program(row);
        // Reopened ROMs can contain changed scene/dialogue resources without
        // pending project edits. Narrative meanings require the researched base.
        const bool originalScenes = sha256(project_->runtimeRom().bytes()) ==
            "571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a" &&
            !project_->scenePrograms()->modified() &&
            (!project_->dialogues() || !project_->dialogues()->modified());
        const bool replaced=project_->sceneEvents()->flagEdits()[QString::number(row)].toObject()["replace"].toBool();
        const auto handlers = replaced?EventScript::decode(bytes,row):EventScript::display(bytes, row, originalScenes);
        const auto retailOffsets=replaced?QSet<int>{}:EventScript::instructionOffsets(row);
        QMap<int, QString> sceneNames;
        for (const auto &id : project_->scenePrograms()->scenes())
            for (int selector : project_->scenePrograms()->resource(id).selectors)
                sceneNames[selector] = project_->scenePrograms()->displayName(id);
        QMap<int, SceneEventSite> launches;
        for (const auto &site : SceneEvents::sites()) if (site.row == row) launches[site.offset] = site;
        int handlerNumber = 0;
        for (const auto &handler : handlers) {
            auto parent = new QTreeWidgetItem(steps_, {QString::number(++handlerNumber),
                (handler.title.isEmpty() ? EventScript::groupName(handler.group) : handler.title) + " · " + position(handler.entry)});
            parent->setData(0, offsetRole, -1); parent->setExpanded(true);
            for (const auto &step : handler.steps) {
                if (!all_->isChecked() && step.flag < 0 && step.target<0 && !launches.contains(step.offset)) continue;
                QString text = step.text;
                if (launches.contains(step.offset) && !step.researched) {
                    const auto &site = launches[step.offset];
                    const int selector = project_->sceneEvents()->target(site.id);
                    text = QString("Play cutscene %1 · %2").arg(selector).arg(sceneNames.value(selector, site.name));
                }
                QString value=QString::number(u8(bytes,step.offset+1))+" · Edit…";
                const int opcode=u8(bytes,step.offset);
                if(opcode==0||opcode==4||opcode==0x20||opcode==0x29||opcode==0x2f)value="—";
                if(step.flag>=0)value=QString("Flag %1 · %2").arg(step.flag).arg(step.value?"ON":"OFF");
                else if(step.target>=0)value=position(step.target)+" · Choose destination…";
                else if(u8(bytes,step.offset)>=0x68&&u8(bytes,step.offset)<=0x6f&&step.offset+3<bytes.size()&&u8(bytes,step.offset+2)==u8(bytes,step.offset)+0x20)
                    value=QString::number(u8(bytes,step.offset+1)|(u8(bytes,step.offset+3)<<8))+" · Edit…";
                if(!retailOffsets.contains(step.offset))value="Edit in Events";
                auto item = new QTreeWidgetItem(parent, {position(step.offset), text,value});
                item->setData(0, offsetRole, step.offset); item->setData(0, flagRole, step.flag);
                item->setData(0, checkRole, step.check); item->setData(0, valueRole, step.value);
                item->setData(0, targetRole, step.target);
                item->setData(0, Qt::UserRole + 5, step.flagLabel);
                item->setToolTip(1, text + "\nOriginal instruction location: " + position(step.offset));
                if (step.offset == previous) steps_->setCurrentItem(item);
            }
            if (!parent->childCount()) delete parent;
        }
        notice_->clear();
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); restore_->setEnabled(false); }
    selectStep();
}
void ScenarioEventEditor::selectStep() {
    const QScopedValueRollback<bool> guard(loading_, true);
    auto item = steps_->currentItem();
    bool launch=false;
    if(item&&item->parent())for(const auto &site:SceneEvents::sites())launch|=site.row==rows_->currentData().toInt()&&site.offset==item->data(0,offsetRole).toInt();
    openCutscene_->setEnabled(launch&&bool(editCutscene));
    branchTarget_->clear();
    const bool branch = item && item->parent() && item->data(0,targetRole).toInt()>=0;
    if(branch) {
        try {
            const int row=rows_->currentData().toInt(),at=item->data(0,offsetRole).toInt();
            const auto original=SceneEvents::retailRow(project_->rom(),row);
            const auto controls=EventScript::branchSteps(original);
            if(controls.contains(at)) {
                const int end=at+controls[at].length;
                QSet<int> listed;
                for(const auto &handler:EventScript::decode(original, row))for(const auto &step:handler.steps) {
                    const int distance=(step.offset-end)/2;
                    if(distance < -128 || distance > 127 || listed.contains(step.offset))continue;
                    listed.insert(step.offset);
                    branchTarget_->addItem(position(step.offset)+" · "+step.text,step.offset);
                }
                branchTarget_->setCurrentIndex(branchTarget_->findData(item->data(0,targetRole)));
            }
        } catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));}
    }
    branchTarget_->setEnabled(branchTarget_->count()>0);applyBranch_->setEnabled(branchTarget_->count()>0);
    const bool editable = item && item->parent() && item->data(0, flagRole).toInt() >= 0;
    flagPanel_->setVisible(editable);branchPanel_->setVisible(editable&&branch);
    apply_->setVisible(editable);
    showParameters(editable?nullptr:item);
    flags_->setEnabled(editable); action_->setEnabled(editable); apply_->setEnabled(editable);
    jump_->setEnabled(item && item->parent() && item->data(0, targetRole).toInt() >= 0);
    action_->clear();
    discard_->hide();
    selection_ = editable ? QPair(rows_->currentData().toInt(), item->data(0, offsetRole).toInt()) : QPair(-1, -1);
    if (!editable) { detail_->setText(item&&item->parent()?QString{}:"Select an instruction to edit its values."); return; }
    const bool check = item->data(0, checkRole).toBool();
    for (int i = 0; i < flags_->count(); ++i)
        flags_->setItemText(i, EventScript::flagName(flags_->itemData(i).toInt(), rows_->currentData().toInt()));
    flags_->setCurrentIndex(flags_->findData(item->data(0, flagRole)));
    const auto scopedName = item->data(0, Qt::UserRole + 5).toString();
    if (!scopedName.isEmpty()) flags_->setItemText(flags_->currentIndex(), scopedName);
    action_->addItem(check ? "Check OFF — take the branch when OFF" : "Clear — turn OFF", false);
    action_->addItem(check ? "Check ON — take the branch when ON" : "Set — turn ON", true);
    action_->setCurrentIndex(action_->findData(item->data(0, valueRole)));
    detail_->setText(item->text(1) + (check ? "\nThe other result continues after this check. Use Go to branch destination to inspect its target." : "\nThis changes the flag when the game reaches this step."));
    if (drafts_.contains(selection_)) {
        const auto &draft = drafts_[selection_];
        if (draft.flag >= 0) flags_->setCurrentIndex(flags_->findData(draft.flag));
        else flags_->setEditText(draft.text);
        action_->setCurrentIndex(action_->findData(draft.value));
        discard_->show();
        notice_->setText("Pending flag changes — Apply or Discard. Switching steps retains this draft.");
    }
}
void ScenarioEventEditor::rememberDraft() {
    if (loading_ || selection_.first < 0 || action_->currentIndex() < 0) return;
    auto item = steps_->currentItem(); if (!item) return;
    const int flag = flags_->currentText() == flags_->itemText(flags_->currentIndex()) ? flags_->currentData().toInt() : -1;
    const bool value = action_->currentData().toBool();
    const int baseFlag = item->data(0, flagRole).toInt(); const bool baseValue = item->data(0, valueRole).toBool();
    if (flag == baseFlag && value == baseValue) drafts_.remove(selection_);
    else {
        const auto previous = drafts_.value(selection_, {flag, baseFlag, value, baseValue, flags_->currentText()});
        drafts_[selection_] = {flag, previous.baseFlag, value, previous.baseValue, flags_->currentText()};
    }
    discard_->setVisible(drafts_.contains(selection_));
    notice_->setText(drafts_.contains(selection_) ? "Pending flag changes — Apply or Discard. Switching steps retains this draft." : "");
    pending_->notify();
}
void ScenarioEventEditor::applyDraftTo(SceneEvents &next, QPair<int, int> key) {
    const auto &draft = drafts_[key];
    if (draft.flag < 0) throw Error("Choose a listed flag before applying.");
    const auto bytes = SceneEvents::flagRow(SceneEvents::retailRow(project_->rom(), key.first), key.first, project_->sceneEvents()->flagEdits());
    bool current = false;
    for (const auto &handler : EventScript::decode(bytes)) for (const auto &step : handler.steps)
        if (step.offset == key.second && step.flag == draft.baseFlag && step.value == draft.baseValue) current = true;
    if (!current) throw Error("A pending flag conflicts with another edit. Discard the draft or undo that edit first.");
    next.editFlag(key.first, key.second, draft.flag, draft.value);
}
bool ScenarioEventEditor::applyDrafts() {
    const auto priorDrafts = drafts_;
    try {
        auto next = *project_->sceneEvents();
        for (const auto &key : drafts_.keys()) applyDraftTo(next, key);
        drafts_.clear(); commit(next, "Apply pending event flags"); return true;
    } catch (const std::exception &e) { drafts_ = priorDrafts; notice_->setText(QString::fromUtf8(e.what())); return false; }
}
void ScenarioEventEditor::apply() {
    if (!apply_->isEnabled()) return;
    const auto priorDrafts = drafts_;
    try {
        if (flags_->currentText() != flags_->itemText(flags_->currentIndex())) throw Error("Choose a listed flag before applying");
        auto next = *project_->sceneEvents();
        if (drafts_.contains(selection_)) { applyDraftTo(next, selection_); drafts_.remove(selection_); }
        commit(next, "Edit scenario event flag");
    } catch (const std::exception &e) { drafts_ = priorDrafts; notice_->setText(QString::fromUtf8(e.what())); }
}
void ScenarioEventEditor::commit(const SceneEvents &next, const QString &label) {
    if (next.json() != project_->sceneEvents()->json()) undo_->push(new FlagCommand(project_, next.json(), label));
    showRow(); changed_(); pending_->notify();
}
void ScenarioEventEditor::clearParameters() {
    if(parameterItem_){steps_->removeItemWidget(parameterItem_,2);parameterItem_->setSizeHint(2,QSize{});
        parameterItem_->setText(2,parameterItem_->data(2,Qt::UserRole).toString());}
    if(parameterWidget_){parameterWidget_->hide();parameterWidget_->deleteLater();}
    parameterWidget_=nullptr;parameterItem_=nullptr;
}
void ScenarioEventEditor::showParameters(QTreeWidgetItem *item) {
    clearParameters();if(!item||!item->parent())return;
    const int row=rows_->currentData().toInt(),offset=item->data(0,offsetRole).toInt();
    try {
        const auto bytes=SceneEvents::flagRow(SceneEvents::retailRow(project_->rom(),row),row,project_->sceneEvents()->flagEdits());
        auto fields=EventScript::parameters(bytes,row,offset);
        if(fields.isEmpty())return;
        for(auto &f:fields)if(f.kind=="identity-class"||f.kind=="equipment"||f.kind=="consumable") {
            const auto table=f.kind=="identity-class"?Table::Classes:f.kind=="equipment"?Table::Items:Table::Consumables;
            f.choices.clear();for(const auto &record:project_->records(table))
                if(record.id>=f.minimum&&record.id<=f.maximum)f.choices[record.id]=record.name;
        }
        for(auto &f:fields)if(f.key=="destination")
            for(int h=0;h<steps_->topLevelItemCount();++h) {
                const auto handler=steps_->topLevelItem(h);
                for(int n=0;n<handler->childCount();++n) {
                    const auto step=handler->child(n);const int at=step->data(0,offsetRole).toInt();
                    if(f.choices.contains(at))f.choices[at]=position(at)+" · "+step->text(1).left(120);
                }
            }
        for(const auto &site:SceneEvents::sites())if(site.row==row&&offset==site.offset-(site.shortForm?2:4)) {
            for(auto &f:fields)if(f.key=="value") {
                f.label="Cutscene";f.maximum=65535;f.value=project_->sceneEvents()->target(site.id);f.choices.clear();
                for(const auto &id:project_->scenePrograms()->scenes())for(int selector:project_->scenePrograms()->resource(id).selectors)
                    f.choices[selector]=project_->scenePrograms()->displayName(id);
            }
        }
        parameterWidget_=new QWidget;parameterWidget_->setAutoFillBackground(true);parameterItem_=item;
        item->setData(2,Qt::UserRole,item->text(2));item->setText(2,{});
        auto form=new QFormLayout(parameterWidget_);form->setContentsMargins(5,3,5,3);
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        auto change=[this,row,offset](const QString &key,int value) {
            // Finish the widget's event before refreshing the instruction tree.
            QTimer::singleShot(0,this,[this,row,offset,key,value] {
                try {
                    auto next=*project_->sceneEvents();next.editParameter(row,offset,key,value,*project_->scenePrograms());
                    commit(next,"Edit event instruction value");
                }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
            });
        };
        for(const auto &f:fields) {
            const auto name="scenarioEventParameter_"+f.key;
            if(f.choices.isEmpty()) {
                auto number=new QSpinBox;number->setObjectName(name);number->setRange(f.minimum,f.maximum);
                number->setKeyboardTracking(false);number->setValue(f.value);form->addRow(f.label,number);
                connect(number,&QSpinBox::valueChanged,this,[change,key=f.key](int value){change(key,value);});
            }else {
                auto choice=new QComboBox;choice->setObjectName(name);choice->setMinimumContentsLength(15);
                choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                choice->setEditable(true);choice->setInsertPolicy(QComboBox::NoInsert);
                for(auto it=f.choices.begin();it!=f.choices.end();++it)choice->addItem(QString::number(it.key())+" · "+it.value(),it.key());
                if(choice->findData(f.value)<0)choice->addItem(QString("Existing ID %1").arg(f.value),f.value);
                choice->setCurrentIndex(choice->findData(f.value));choice->completer()->setFilterMode(Qt::MatchContains);
                choice->completer()->setCaseSensitivity(Qt::CaseInsensitive);
                form->addRow(f.label,choice);
                connect(choice,&QComboBox::activated,this,[change,choice,key=f.key](int index){change(key,choice->itemData(index).toInt());});
            }
        }
        item->setSizeHint(2,parameterWidget_->sizeHint());steps_->setItemWidget(item,2,parameterWidget_);
        steps_->doItemsLayout();steps_->scrollToItem(item);
    }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
}
} // namespace lordly
