#include "app/event_flow_editor.h"
#include "app/event_tree_dialog.h"
#include "app/scenario_flowchart_model.h"
#include "app/event_flag_links.h"
#include "engine/event_authoring.h"
#include "engine/event_tree.h"
#include <QComboBox>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QCompleter>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStringList>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QMessageBox>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class EventEdit final:public QUndoCommand {
    Project *project_;QJsonObject before_,after_;
  public:
    EventEdit(Project *p,QJsonObject after):QUndoCommand("Edit story event"),project_(p),before_(p->sceneEvents()->json()),after_(after){}
    void undo()override{project_->setSceneEvents(before_);}void redo()override{project_->setSceneEvents(after_);}
};
QLabel *label(const QString &text,QWidget *parent=nullptr){auto result=new QLabel(text,parent);result->setWordWrap(true);return result;}
bool canChangeTrigger(int row,const StoryEvent &event){
    return EventAuthoring::supported(row)&&row!=0&&row!=70&&event.group>=0&&event.group<=5&&event.authoredId.isEmpty();
}
}
EventFlowEditor::EventFlowEditor(Project *p,QUndoStack *undo,std::function<void()> changed,QWidget *parent)
    :QWidget(parent),project_(p),undo_(undo),changed_(std::move(changed)) {
    setObjectName("storyEventsView");auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto split=new QSplitter;layout->addWidget(split,1);
    auto left=new QWidget;auto leftLayout=new QVBoxLayout(left);leftLayout->setContentsMargins(0,0,0,0);
    search_=new QLineEdit;search_->setObjectName("storyEventSearch");search_->setPlaceholderText("Find a town, character or event…");leftLayout->addWidget(search_);
    list_=new QListWidget;list_->setObjectName("storyEventList");list_->setWordWrap(true);list_->setTextElideMode(Qt::ElideNone);leftLayout->addWidget(list_,1);split->addWidget(left);
    auto add=new QPushButton("Create event…");add->setObjectName("storyCreateEvent");add->setEnabled(false);leftLayout->addWidget(add);connect(add,&QPushButton::clicked,this,[this]{editTree();});
    auto flags=new QPushButton("Flags…");flags->setObjectName("storyFlags");leftLayout->addWidget(flags);connect(flags,&QPushButton::clicked,this,[this]{editFlags();});
    scroll_=new QScrollArea;scroll_->setObjectName("storyEventDetails");scroll_->setWidgetResizable(true);split->addWidget(scroll_);split->setStretchFactor(0,0);split->setStretchFactor(1,1);split->setSizes({310,900});
    notice_=label({});notice_->setObjectName("storyEventNotice");layout->addWidget(notice_);
    connect(search_,&QLineEdit::textChanged,this,[this]{filter();});
    connect(list_,&QListWidget::currentRowChanged,this,[this]{page_=0;focusOffset_=-1;
        revealedEntry_=list_->currentRow()>=0&&list_->currentRow()<events_.size()?events_[list_->currentRow()].entry:-1;
        selectedAuthoredId_=list_->currentRow()>=0&&list_->currentRow()<events_.size()?events_[list_->currentRow()].authoredId:QString{};showEvent();});
}
void EventFlowEditor::setRow(int row,int entryFilter) {
    const int entry=row_==row&&list_->currentRow()>=0&&list_->currentRow()<events_.size()?events_[list_->currentRow()].entry:-1;
    if(row_!=row||entryFilter_!=entryFilter){focusOffset_=-1;revealedEntry_=-1;if(row_!=row)selectedAuthoredId_.clear();}
    entryFilter_=entryFilter;
    if(row_!=row)page_=0;row_=row;events_.clear();program_.clear();notice_->clear();const QSignalBlocker block(list_);list_->clear();
    findChild<QPushButton *>("storyCreateEvent")->setEnabled(EventAuthoring::supported(row));
    try {
        if(row>=0) {
            program_=project_->sceneEvents()->program(row);
            const bool originalScenes=!project_->scenePrograms()->modified()&&(!project_->dialogues()||!project_->dialogues()->modified())&&
                sha256(project_->runtimeRom().bytes())=="571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a";
            project_->loadCampaignMap();
            events_=EventFlow::read(project_->rom(),*project_->sceneEvents(),*project_->scenePrograms(),row,originalScenes,true,&*project_->campaignMap());
        }
        int selected=0;for(int n=0;n<events_.size();++n){const auto &event=events_[n];
            auto item=new QListWidgetItem(event.title+"\n"+event.trigger,list_);item->setData(Qt::UserRole,event.entry);
            QString search=event.title+" "+event.trigger;for(const auto &action:event.actions)search+=" "+action.title+" "+action.detail+" "+storyConditionText(action.condition);
            for(const auto &condition:event.requirements)search+=" "+condition.title+" "+storyConditionText(condition.condition);
            item->setData(Qt::UserRole+1,search);const int preferred=revealedEntry_>=0?revealedEntry_:entryFilter_>=0?entryFilter_:entry;
            if(event.entry==preferred)selected=n;}
        if(!selectedAuthoredId_.isEmpty())for(int n=0;n<events_.size();++n)if(events_[n].authoredId==selectedAuthoredId_){selected=n;revealedEntry_=events_[n].entry;break;}
        list_->setCurrentRow(events_.isEmpty()?-1:selected);filter();showEvent();
    }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
}
void EventFlowEditor::filter(){int first=-1;for(int n=0;n<list_->count();++n){const int at=list_->item(n)->data(Qt::UserRole).toInt();const bool hidden=(entryFilter_>=0&&at!=entryFilter_&&at!=revealedEntry_)||!list_->item(n)->data(Qt::UserRole+1).toString().contains(search_->text(),Qt::CaseInsensitive);list_->item(n)->setHidden(hidden);if(!hidden&&first<0)first=n;}
    if(list_->currentItem()&&list_->currentItem()->isHidden())list_->setCurrentRow(first);}
int EventFlowEditor::selectedOffset()const{return focusOffset_>=0?focusOffset_:list_->currentRow()>=0&&list_->currentRow()<events_.size()?events_[list_->currentRow()].entry:-1;}
QString EventFlowEditor::destination(int offset)const{
    QSet<int> seen;QString fallback;
    while(!seen.contains(offset)){
        seen.insert(offset);const StoryAction *matched=nullptr;
        for(const auto &event:events_){
            if(event.triggerInstructions.contains(offset))return event.title;
            for(const auto &condition:event.requirements)if(condition.instructions.contains(offset))
                return condition.condition.operation.isEmpty()?condition.title:storyConditionText(condition.condition);
            for(const auto &action:event.actions)if(action.instructions.contains(offset)){matched=&action;break;}
            if(matched)break;
            if(event.entry==offset)return event.title;
        }
        if(!matched)break;
        fallback=matched->title;
        // Skip only proven unconditional local jumps. Calls keep their own
        // semantic title; named shared routes retain their helper summary too.
        const int last=matched->instructions.isEmpty()?-1:matched->instructions.last();
        const bool genericRoute=matched->title=="Continue at another step"||matched->title=="Continue in shared event logic";
        if(matched->kind=="route"&&genericRoute&&matched->target>=0&&last>=0&&last<program_.size()&&
            (u8(program_,last)==1||u8(program_,last)==2)&&!seen.contains(matched->target)){
            offset=matched->target;continue;
        }
        return matched->condition.operation.isEmpty()?matched->title:storyConditionText(matched->condition);
    }
    return fallback.isEmpty()?QString("Event step (see raw instructions)"):fallback;
}
void EventFlowEditor::selectOffset(int offset){
    for(int n=0;n<events_.size();++n){const auto &event=events_[n];bool found=event.entry==offset||event.triggerInstructions.contains(offset);
        if(!event.authoredId.isEmpty()&&offset>=event.entry){
            int end=program_.size();for(const auto &other:events_)if(other.entry>event.entry)end=qMin(end,other.entry);
            found|=offset<end;
        }
        int selectedPage=0,anchor=offset;
        for(int a=0;a<event.actions.size();++a)if(event.actions[a].instructions.contains(offset)){found=true;selectedPage=a/40;anchor=event.actions[a].instructions.first();}
        for(const auto &condition:event.requirements)found|=condition.instructions.contains(offset);
        if(!found)continue;const QSignalBlocker block(list_);revealedEntry_=event.entry;selectedAuthoredId_=event.authoredId;focusOffset_=offset;search_->clear();filter();list_->setCurrentRow(n);page_=selectedPage;showEvent();
        if(auto widget=scroll_->widget()->findChild<QWidget *>(QString("storyAction_%1").arg(anchor)))scroll_->ensureWidgetVisible(widget);
        return;}
}
void EventFlowEditor::addField(QFormLayout *form,StoryField field){
    if(field.kind=="scenario-site"||field.kind=="scenario-squad"){field.choices=eventTargets(project_,row_,field.kind=="scenario-site"?2:3);field.choices.remove(-1);}
    if(field.key=="comparison"&&field.kind!="named-state"){
        field.kind="named-state";field.choices={{10,"Equals"},{11,"Does not equal"},{12,"Greater than"},{13,"Less than"},{14,"At least"},{15,"At most"}};
    }
    if(field.kind=="identity-class"||field.kind=="equipment"||field.kind=="consumable"){
        const auto table=field.kind=="identity-class"?Table::Classes:field.kind=="equipment"?Table::Items:Table::Consumables;
        field.choices.clear();for(const auto &r:project_->records(table))if(r.id>=field.minimum&&r.id<=field.maximum)field.choices[r.id]=r.name;
    }
    if(field.key=="destination"){
        QMap<int,QString> starts;
        for(const auto &event:events_){
            starts[event.entry]=event.title;
            for(const auto &requirement:event.requirements)if(!requirement.instructions.isEmpty())starts[requirement.instructions.first()]=event.title;
            for(const auto &action:event.actions)if(!action.instructions.isEmpty())starts[action.instructions.first()]=event.title;
        }
        // Normal editing addresses complete semantic steps. The current native
        // target is retained even when it enters the middle of a grouped step.
        for(auto i=field.choices.begin();i!=field.choices.end();){
            if(!starts.contains(i.key()))i=field.choices.erase(i);
            else{i.value()=destination(i.key());++i;}
        }
        if(!starts.contains(field.value))field.choices[field.value]=QString("Inside %1 (existing target)").arg(destination(field.value));
        else if(!field.choices.contains(field.value))field.choices[field.value]=destination(field.value);
        QMap<QString,int> counts;for(const auto &name:field.choices)++counts[name];
        for(auto i=field.choices.begin();i!=field.choices.end();++i)
            if(counts[i.value()]>1&&starts.value(i.key())!=i.value())i.value()+=" · "+starts.value(i.key());
        counts.clear();for(const auto &name:field.choices)++counts[name];
        for(auto i=field.choices.begin();i!=field.choices.end();++i)
            if(counts[i.value()]>1)i.value()+=QString(" [raw offset %1]").arg(i.key());
    }
    const auto object=QString("storyValue_%1_%2").arg(field.offset).arg(field.key.isEmpty()?"state":field.key)+(field.mode==StoryField::SquadState?QString("_bit%1").arg(field.mask):QString{});
    if(field.choices.isEmpty()){
        auto spin=new QSpinBox;spin->setObjectName(object);spin->setRange(field.minimum,field.maximum);spin->setValue(field.value);spin->setKeyboardTracking(false);form->addRow(field.label,spin);
        connect(spin,&QSpinBox::valueChanged,this,[this,field](int value){change(field,value);});
    }else{
        auto combo=new QComboBox;combo->setObjectName(object);combo->setEditable(true);combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setMinimumContentsLength(25);combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        const bool namedOnly=field.kind=="named-state"||field.key=="destination";
        for(auto i=field.choices.begin();i!=field.choices.end();++i)if(i.key()>=field.minimum&&i.key()<=field.maximum)combo->addItem(i.value()+(namedOnly?QString{}:QString(" [%1]").arg(i.key())),i.key());
        if(combo->findData(field.value)<0)combo->addItem(field.key=="destination"?field.choices.value(field.value,destination(field.value)):QString("Existing ID %1").arg(field.value),field.value);
        combo->setCurrentIndex(combo->findData(field.value));combo->lineEdit()->setCursorPosition(0);
        combo->completer()->setFilterMode(Qt::MatchContains);combo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        form->addRow(field.label,combo);connect(combo,&QComboBox::activated,this,[this,field,combo](int index){change(field,combo->itemData(index).toInt());});
    }
}
void EventFlowEditor::addCondition(QFormLayout *form,const StoryCondition &condition){
    if(condition.operation=="all"||condition.operation=="any"||condition.operation=="not"){
        const auto title=condition.operation=="all"?"ALL OF (AND)":condition.operation=="any"?"ANY OF (OR)":"NOT";
        auto group=new QGroupBox(title);auto nested=new QFormLayout(group);
        group->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
        for(const auto &child:condition.children){
            if(child.operation==condition.operation&&condition.operation!="not")for(const auto &part:child.children)addCondition(nested,part);
            else addCondition(nested,child);
        }
        form->addRow(group);
    }else{
        form->addRow(label(condition.title));
        for(const auto &field:condition.fields){
            addField(form,field);
            if(field.key=="destination"){
                auto open=new QPushButton("Open shared check");open->setObjectName(QString("storyOpenCheck_%1").arg(field.value));form->addRow(open);
                connect(open,&QPushButton::clicked,this,[this,at=field.value]{selectOffset(at);});
            }
        }
        for(const auto &field:condition.fields)if(field.kind=="flag"||field.label=="Flag")
            form->addRow(eventFlagLinks(project_,row_,field.value,[this](int row,int at){if(openEventInstruction)openEventInstruction(row,at);else if(row==row_)selectOffset(at);}));
    }
}
void EventFlowEditor::showEvent(){
    if(auto old=scroll_->takeWidget()){old->hide();old->deleteLater();}
    auto content=new QWidget;auto layout=new QVBoxLayout(content);layout->setSizeConstraint(QLayout::SetMinAndMaxSize);scroll_->setWidget(content);
    if(list_->currentRow()<0||list_->currentRow()>=events_.size()){layout->addWidget(label("Select an event."));layout->addStretch();return;}
    const auto &event=events_[list_->currentRow()];
    auto heading=label(event.title);auto font=heading->font();font.setBold(true);font.setPointSize(font.pointSize()+2);heading->setFont(font);layout->addWidget(heading);
    if(event.group==5) {
        auto common=new QGroupBox("Standard scenario closing logic · game-run");
        auto box=new QVBoxLayout(common);
        auto outline=new QTreeWidget;outline->setObjectName("storyMissionEndTree");
        outline->setHeaderHidden(true);outline->setWordWrap(true);outline->setTextElideMode(Qt::ElideNone);outline->setIndentation(16);
        outline->setMinimumHeight(320);outline->setMaximumHeight(420);box->addWidget(outline);
        auto detail=label({});detail->setObjectName("storyMissionEndDetail");box->addWidget(detail);
        const auto graph=missionEndFlow(*project_);QMap<int,QTreeWidgetItem *> first;
        int firstStep=0,lastStep=graph.steps.size();
        std::function<void(int,QTreeWidgetItem *)> show=[&](int at,QTreeWidgetItem *parent) {
            const auto &step=graph.steps[at];const bool again=first.contains(at)||at<firstStep||at>=lastStep;
            auto item=new QTreeWidgetItem(parent,{(again?"Continue → ":step.decision?"IF  ":"")+step.title});
            item->setData(0,Qt::UserRole,at);item->setData(0,Qt::UserRole+1,again);item->setToolTip(0,step.detail);
            if(again)return;first[at]=item;
            for(const auto &edge:graph.edges)if(edge.from==at) {
                auto branch=edge.label.isEmpty()?parent:new QTreeWidgetItem(item,{edge.label=="Yes"?"THEN":edge.label=="No"?"ELSE":edge.label});
                show(edge.to,branch);
            }
        };
        if(graph.sections.isEmpty()){if(!graph.steps.isEmpty())show(0,outline->invisibleRootItem());}
        else for(int part=0;part<graph.sections.size();++part) {
            firstStep=graph.sections[part].second;
            lastStep=part+1<graph.sections.size()?graph.sections[part+1].second:graph.steps.size();
            auto heading=new QTreeWidgetItem(outline,{graph.sections[part].first});
            auto font=heading->font(0);font.setBold(true);heading->setFont(0,font);show(firstStep,heading);
        }
        outline->expandAll();
        connect(outline,&QTreeWidget::currentItemChanged,common,[detail,graph](QTreeWidgetItem *item) {
            if(!item||!item->data(0,Qt::UserRole).isValid()){detail->clear();return;}
            const int at=item->data(0,Qt::UserRole).toInt();detail->setText(graph.steps[at].detail);
        });
        connect(outline,&QTreeWidget::itemDoubleClicked,common,[outline,first](QTreeWidgetItem *item) {
            if(!item||!item->data(0,Qt::UserRole+1).toBool())return;
            auto destination=first.value(item->data(0,Qt::UserRole).toInt());
            if(!destination)return;
            for(auto parent=destination->parent();parent;parent=parent->parent())parent->setExpanded(true);
            outline->setCurrentItem(destination);outline->scrollToItem(destination);
        });
        layout->addWidget(common);
    }
    const auto gates=project_->sceneEvents()->flagEdits()[QString::number(row_)].toObject()["gates"].toObject();
    bool gated=false;
    for(auto gate=gates.begin();gate!=gates.end();++gate){
        const int pc=gate.key().toInt();gated|=event.triggerInstructions.contains(pc);
        for(const auto &action:event.requirements+event.actions)gated|=action.instructions.contains(pc);
    }
    if(gated){
        // Both views use the same edited graph, including bypassed native guards.
        const auto charts=scenarioChart(*project_,{{"Mission",{row_,event.entry,{}}}});
        const auto graph=std::find_if(charts.cbegin(),charts.cend(),[&](const auto &g){return g.address.offset==event.entry;});
        if(graph!=charts.cend()){
            layout->addWidget(label("WHEN · "+graph->trigger));
            auto outline=new QTreeWidget;outline->setHeaderHidden(true);outline->setObjectName("storyCompactTree");
            outline->setWordWrap(true);layout->addWidget(outline);
            QSet<int> visited;
            std::function<void(int,QTreeWidgetItem *)> show=[&](int step,QTreeWidgetItem *parent){
                if(step<0||step>=graph->steps.size())return;
                const auto &s=graph->steps[step];auto item=new QTreeWidgetItem(parent,{(visited.contains(step)?"Continue → ":s.decision?"IF ":"")+s.title});
                item->setData(0,Qt::UserRole,step);if(visited.contains(step))return;visited.insert(step);
                for(const auto &edge:graph->edges)if(edge.from==step){
                    auto branch=edge.label.isEmpty()?parent:new QTreeWidgetItem(item,{edge.label=="Yes"?"THEN":edge.label=="No"?"ELSE":edge.label});
                    show(edge.to,branch);
                }
            };
            if(!graph->steps.isEmpty())show(0,outline->invisibleRootItem());
            outline->expandAll();outline->setMinimumHeight(200);
            auto details=new QWidget;auto form=new QFormLayout(details);layout->addWidget(details);
            const auto native=EventFlow::read(project_->rom(),*project_->sceneEvents(),*project_->scenePrograms(),row_,false,false);
            auto inspect=[this,form,graph=*graph,native,group=event.group](QTreeWidgetItem *item){
                while(form->rowCount())form->removeRow(0);
                if(!item||!item->data(0,Qt::UserRole).isValid())return;
                const auto &step=graph.steps[item->data(0,Qt::UserRole).toInt()];const auto address=step.address;
                if(address.gateControl){
                    const auto old=project_->sceneEvents()->flagEdits()[QString::number(row_)].toObject()["gates"].toObject()[QString::number(address.gate)].toObject();
                    auto edit=new QPushButton(old["mode"]=="keep"?"Restore condition":"Edit condition…");form->addRow(edit);
                    connect(edit,&QPushButton::clicked,this,[this,address,old,group]{
                        try{
                            auto next=*project_->sceneEvents();QJsonObject gate,flags;
                            auto translate=[&](QJsonObject value){if(address.gateReversed)for(const QString key:{"yes","no"}){if(value[key]=="yes")value[key]="no";else if(value[key]=="no")value[key]="yes";}return value;};
                            if(old["mode"]=="if"){auto result=editRetailCondition(project_,row_,group,translate(old),this,&flags);if(!result)return;gate=translate(*result);next.apply(flags,*project_->scenePrograms());}
                            next.setRetailGate(row_,address.gate,gate,*project_->scenePrograms());undo_->push(new EventEdit(project_,next.json()));changed_();
                        }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
                    });
                }else for(const auto &e:native){
                    if(e.triggerInstructions.contains(address.offset)){for(const auto &f:e.triggerFields)addField(form,f);break;}
                    bool found=false;
                    for(const auto &a:e.requirements+e.actions)if(a.instructions.contains(address.offset)){
                        if(!a.condition.operation.isEmpty())addCondition(form,a.condition);
                        for(const auto &f:a.fields)addField(form,f);
                        if(a.scene>=0&&editCutscene){auto edit=new QPushButton("Edit cutscene…");form->addRow(edit);connect(edit,&QPushButton::clicked,this,[this,id=a.scene]{editCutscene(id);});}
                        found=true;break;
                    }if(found)break;
                }
                for(const auto &flag:step.flags)form->addRow(eventFlagLinks(project_,row_,flag.id,[this](int row,int at){if(openEventInstruction)openEventInstruction(row,at);}));
            };
            connect(outline,&QTreeWidget::currentItemChanged,details,inspect);
            outline->setCurrentItem(outline->topLevelItem(0));layout->addStretch();return;
        }
    }
    auto when=new QGroupBox("WHEN");auto triggerForm=new QFormLayout(when);triggerForm->addRow(label(event.trigger));
    if(!event.authoredId.isEmpty()){
        const auto trees=project_->sceneEvents()->flagEdits()[QString::number(row_)].toObject()["trees"].toArray();QJsonObject tree;
        for(const auto &v:trees)if(v.toObject()["id"]==event.authoredId)tree=v.toObject();
        if(event.group>=1&&event.group<=4)triggerForm->addRow(label(eventTargets(project_,row_,event.group).value(tree["target"].toInt(-1),QString("Target ID %1").arg(tree["target"].toInt(-1)))));layout->addWidget(when);
        auto controls=new QHBoxLayout;auto edit=new QPushButton("Edit logic tree…"),copy=new QPushButton("Duplicate event…"),remove=new QPushButton("Delete event");controls->addWidget(edit);controls->addWidget(copy);controls->addWidget(remove);layout->addLayout(controls);
        connect(edit,&QPushButton::clicked,this,[this,id=event.authoredId]{editTree(id);});
        connect(copy,&QPushButton::clicked,this,[this,tree]{try{auto draft=tree;draft.remove("id");draft["name"]=tree["name"].toString()+" copy";QJsonObject flags;
            if(auto result=editEventTree(project_,row_,draft,this,&flags)){auto events=*project_->sceneEvents();events.apply(flags,*project_->scenePrograms());auto trees=events.flagEdits()[QString::number(row_)].toObject()["trees"].toArray();trees<<*result;events.setTrees(row_,trees,*project_->scenePrograms());selectedAuthoredId_=(*result)["id"].toString();search_->clear();undo_->push(new EventEdit(project_,events.json()));changed_();}}catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}});
        connect(remove,&QPushButton::clicked,this,[this,id=event.authoredId]{try{auto events=*project_->sceneEvents();auto trees=events.flagEdits()[QString::number(row_)].toObject()["trees"].toArray();for(int i=0;i<trees.size();++i)if(trees[i].toObject()["id"]==id){trees.removeAt(i);break;}events.setTrees(row_,trees,*project_->scenePrograms());undo_->push(new EventEdit(project_,events.json()));changed_();}catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}});
        auto preview=new QTreeWidget;preview->setObjectName("storyAuthoredTree");preview->setColumnCount(2);preview->setHeaderHidden(true);preview->setWordWrap(true);layout->addWidget(preview);
        preview->header()->setSectionResizeMode(0,QHeaderView::Stretch);preview->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        std::function<void(QTreeWidgetItem *,const QJsonObject &)> showCondition=[&](QTreeWidgetItem *parent,const QJsonObject &condition){
            const auto type=condition["type"].toString();auto item=new QTreeWidgetItem(parent,{type=="all"?"ALL OF (AND)":type=="any"?"ANY OF (OR)":type=="not"?"NOT":eventConditionSummary(condition,row_,&*project_->sceneEvents())});
            if(type=="all"||type=="any")for(const auto &v:condition["conditions"].toArray())showCondition(item,v.toObject());
            else if(type=="not")showCondition(item,condition["condition"].toObject());
            else if(type=="flag")preview->setItemWidget(item,1,eventFlagLinks(project_,row_,condition["flag"].toInt(),[this](int row,int at){if(openEventInstruction)openEventInstruction(row,at);else if(row==row_)selectOffset(at);}));
        };
        std::function<void(QTreeWidgetItem *,QJsonArray)> show=[&](QTreeWidgetItem *parent,QJsonArray nodes){for(const auto &v:nodes){auto n=v.toObject();auto item=new QTreeWidgetItem(parent,{eventTreeSummary(n,row_,&*project_->sceneEvents(),project_)});if(n["type"]=="if"){
            showCondition(item,n["condition"].toObject());
            show(new QTreeWidgetItem(item,{"THEN"}),n["then"].toArray());show(new QTreeWidgetItem(item,{"ELSE"}),n["else"].toArray());}
            else if(n["type"]=="flag")preview->setItemWidget(item,1,eventFlagLinks(project_,row_,n["flag"].toInt(),[this](int row,int at){if(openEventInstruction)openEventInstruction(row,at);}));
        }};
        show(preview->invisibleRootItem(),tree["nodes"].toArray());preview->expandAll();
        connect(preview,&QTreeWidget::itemDoubleClicked,this,[this,preview,id=event.authoredId](QTreeWidgetItem *item){
            QVector<int> path;
            while(item){auto parent=item->parent();path.prepend(parent?parent->indexOfChild(item):preview->indexOfTopLevelItem(item));item=parent;}
            editTree(id,path);
        });
        layout->addStretch();return;
    }
    if(canChangeTrigger(row_,event)){auto change=new QPushButton("Change WHEN…");change->setObjectName("storyChangeWhen");triggerForm->addRow(change);connect(change,&QPushButton::clicked,this,[this]{editWhen();});}
    for(const auto &f:event.triggerFields)addField(triggerForm,f);
    when->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);layout->addWidget(when);
    auto outline=new QTreeWidget;outline->setObjectName("storyCompactTree");outline->setHeaderHidden(true);
    outline->setWordWrap(true);outline->setTextElideMode(Qt::ElideNone);outline->setIndentation(16);layout->addWidget(outline);
    auto details=new QWidget;auto detailLayout=new QVBoxLayout(details);detailLayout->setContentsMargins(0,0,0,0);layout->addWidget(details);
    struct Step {StoryAction action;StoryCondition predicate;int offset;bool reference=false;};
    QVector<Step> steps;
    QTreeWidgetItem *selected=nullptr;
    auto add=[&](QTreeWidgetItem *parent,QString text,Step step){
        auto item=new QTreeWidgetItem(parent,{text});item->setData(0,Qt::UserRole,steps.size());
        if(!selected||step.action.instructions.contains(focusOffset_))selected=item;
        steps<<std::move(step);return item;
    };
    auto root=outline->invisibleRootItem();
    auto bodyRoot=root;
    for(const auto &a:event.requirements)
    {
        auto predicate=add(bodyRoot,"IF  "+storyConditionText(a.condition),{a,a.condition,a.instructions.first()});
        bodyRoot=new QTreeWidgetItem(predicate,{"THEN"});bodyRoot->setFlags(bodyRoot->flags()&~Qt::ItemIsSelectable);
        auto skip=new QTreeWidgetItem(predicate,{"ELSE  Skip this event"});skip->setFlags(skip->flags()&~Qt::ItemIsSelectable);
    }
    auto tree=storyTree(event,program_);
    auto containsFocus=[&](auto &&self,const QVector<StoryTreeNode> &nodes)->bool{
        for(const auto &n:nodes)if(n.kind!=StoryTreeNode::Reference&&
            (event.actions[n.action].instructions.contains(focusOffset_)||self(self,n.thenBranch)||self(self,n.elseBranch)))return true;
        return false;
    };
    if(focusOffset_>=0&&!containsFocus(containsFocus,tree.nodes)){
        for(int i=0;i<event.actions.size();++i)if(event.actions[i].instructions.contains(focusOffset_)){
            tree=storyTree(event,program_,focusOffset_);
            if(!containsFocus(containsFocus,tree.nodes)){
                StoryTreeNode omitted;omitted.action=i;omitted.offset=event.actions[i].instructions.first();tree.nodes.prepend(omitted);
            }
            break;
        }
    }
    std::function<void(QTreeWidgetItem *,const QVector<StoryTreeNode>&)> render;
    render=[&](QTreeWidgetItem *parent,const QVector<StoryTreeNode>&nodes){
        for(const auto &n:nodes){
            if(n.kind==StoryTreeNode::Reference){
                add(parent,n.referenceLabel+" → "+destination(n.offset),{{},{},n.offset,true});continue;
            }
            const auto &a=event.actions[n.action];const bool decision=n.kind==StoryTreeNode::Decision;
            auto condition=decision?n.condition:a.condition;
            auto item=add(parent,(decision||a.kind=="requirement"?"IF  ":"")+ (condition.operation.isEmpty()?a.title:storyConditionText(condition)),{a,condition,n.offset});
            if(decision){
                for(auto branch:{qMakePair(QString("THEN"),&n.thenBranch),qMakePair(QString("ELSE"),&n.elseBranch)}){
                    if(branch.second->isEmpty())continue;
                    auto group=new QTreeWidgetItem(item,{branch.first});group->setFlags(group->flags()&~Qt::ItemIsSelectable);render(group,*branch.second);
                }
            }
        }
    };
    render(bodyRoot,tree.nodes);outline->expandAll();outline->setVisible(!steps.isEmpty());outline->setFixedHeight(qBound(65,static_cast<int>(steps.size()+2*event.requirements.size())*27+12,300));
    auto inspect=[this,steps,detailLayout,details,group=event.group](QTreeWidgetItem *item){
        while(auto cell=detailLayout->takeAt(0)){if(auto w=cell->widget()){w->hide();w->deleteLater();}delete cell;}
        if(!item||!item->data(0,Qt::UserRole).isValid())return;
        const auto &step=steps[item->data(0,Qt::UserRole).toInt()];const auto &a=step.action;
        auto box=new QWidget(details);box->setObjectName(QString("storyAction_%1").arg(step.offset));
        detailLayout->addWidget(box);auto form=new QFormLayout(box);form->setContentsMargins(0,2,0,2);
        if(step.reference){auto open=new QPushButton("Open linked step");form->addRow(open);
            connect(open,&QPushButton::clicked,this,[this,at=step.offset]{selectOffset(at);});return;}
        if(!step.predicate.operation.isEmpty())addCondition(form,step.predicate);
        for(const auto &f:a.fields){addField(form,f);
            if(f.kind=="flag"||f.label=="Flag")form->addRow(eventFlagLinks(project_,row_,f.value,[this](int row,int at){if(openEventInstruction)openEventInstruction(row,at);else if(row==row_)selectOffset(at);}));
        }
        if(!a.detail.isEmpty())form->addRow(label(a.detail));
        auto controls=new QHBoxLayout;
        for(int pc:a.instructions){
            const auto gate=project_->sceneEvents()->flagEdits()[QString::number(row_)].toObject()["gates"].toObject()[QString::number(pc)].toObject();
            if(gate.isEmpty())continue;
            auto edit=new QPushButton(gate["mode"]=="keep"?"Restore condition":"Edit added condition…");controls->addWidget(edit);
            connect(edit,&QPushButton::clicked,this,[this,pc,gate,group]{
                try{
                    auto next=*project_->sceneEvents();QJsonObject flags,updated;
                    if(gate["mode"]=="if"){
                        auto result=editRetailCondition(project_,row_,group,gate,this,&flags);if(!result)return;
                        updated=*result;next.apply(flags,*project_->scenePrograms());
                    }
                    next.setRetailGate(row_,pc,updated,*project_->scenePrograms());undo_->push(new EventEdit(project_,next.json()));changed_();
                }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
            });
        }
        if(a.kind=="program"&&!a.fields.isEmpty()&&openEventProgram){auto open=new QPushButton("Open related events");open->setObjectName(QString("storyOpenProgram_%1").arg(a.fields.first().offset));controls->addWidget(open);
            connect(open,&QPushButton::clicked,this,[this,row=a.fields.first().value]{openEventProgram(row);});}
        if(a.scene>=0&&editCutscene){auto open=new QPushButton("Edit cutscene…");controls->addWidget(open);connect(open,&QPushButton::clicked,this,[this,id=a.scene]{editCutscene(id);});}
        if(a.target>=0&&step.predicate.operation.isEmpty()){
            auto link=[&](QString prefix,int at){auto open=new QPushButton(prefix+destination(at));controls->addWidget(open);connect(open,&QPushButton::clicked,this,[this,at]{selectOffset(at);});};
            link(a.otherwise>=0?"Yes → ":"Go to → ",a.target);if(a.otherwise>=0)link("No → ",a.otherwise);
        }
        if(a.advanced||!a.detail.isEmpty()){auto raw=new QPushButton("Raw instructions…");controls->addWidget(raw);
            connect(raw,&QPushButton::clicked,this,[this,at=step.offset]{if(rawInstructions)rawInstructions(at);});}
        if(controls->count())form->addRow(controls);else delete controls;
    };
    outline->setCurrentItem(selected);if(selected)outline->scrollToItem(selected);inspect(selected);
    connect(outline,&QTreeWidget::currentItemChanged,details,[this,inspect,steps](QTreeWidgetItem *item){
        if(item&&item->data(0,Qt::UserRole).isValid())focusOffset_=steps[item->data(0,Qt::UserRole).toInt()].offset;
        inspect(item);
    });
    connect(outline,&QTreeWidget::itemActivated,details,[this,steps](QTreeWidgetItem *item){
        if(!item->data(0,Qt::UserRole).isValid())return;
        const auto &s=steps[item->data(0,Qt::UserRole).toInt()];if(s.reference)selectOffset(s.offset);
    });
    if(focusOffset_>=0&&focusOffset_!=event.entry){auto back=new QPushButton("Back to event start");layout->addWidget(back);
        connect(back,&QPushButton::clicked,this,[this]{focusOffset_=-1;showEvent();});}
    if(!tree.otherEntries.isEmpty()){
        auto other=new QHBoxLayout;auto choices=new QComboBox;choices->setObjectName("storyOtherPaths");
        choices->setMinimumContentsLength(25);choices->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        for(int at:tree.otherEntries)choices->addItem(destination(at),at);
        auto open=new QPushButton("Open path");other->addWidget(new QLabel("Other event paths"));other->addWidget(choices,1);other->addWidget(open);
        connect(open,&QPushButton::clicked,this,[this,choices]{selectOffset(choices->currentData().toInt());});layout->addLayout(other);
    }
    auto raw=new QPushButton("View this event’s raw instructions");layout->addWidget(raw);connect(raw,&QPushButton::clicked,this,[this,at=event.entry]{if(rawInstructions)rawInstructions(at);});layout->addStretch();
}
void EventFlowEditor::change(StoryField field,int value){
    if(chartDraft_){
        try{EventFlow::set(*chartDraft_,*project_->scenePrograms(),row_,field,value);}
        catch(const std::exception &e){QMessageBox::warning(QApplication::activeModalWidget(),"Edit event step",QString::fromUtf8(e.what()));}
        return;
    }
    const int row=row_;QTimer::singleShot(0,this,[this,row,field,value]{
        try{auto next=*project_->sceneEvents();EventFlow::set(next,*project_->scenePrograms(),row,field,value);
            if(next.json()!=project_->sceneEvents()->json())undo_->push(new EventEdit(project_,next.json()));
            changed_();
        }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
    });
}
void EventFlowEditor::editChartStep(int row,int offset,QWidget *owner){
    setRow(row);
    const auto native=EventFlow::read(project_->rom(),*project_->sceneEvents(),*project_->scenePrograms(),row,false,false);
    QDialog dialog(owner);dialog.setWindowTitle("Edit event step");dialog.setMinimumWidth(560);
    auto layout=new QFormLayout(&dialog);bool found=false;
    for(const auto &event:native){
        if(event.triggerInstructions.contains(offset)){
            layout->addRow(label(event.trigger));for(const auto &f:event.triggerFields)addField(layout,f);
            found=true;break;
        }
        for(const auto &step:event.requirements+event.actions){
            if(!step.instructions.contains(offset))continue;
            if(!step.condition.operation.isEmpty())addCondition(layout,step.condition);
            else layout->addRow(label(step.title));
            for(const auto &field:step.fields)addField(layout,field);
            if(!step.detail.isEmpty())layout->addRow(label(step.detail));
            found=true;break;
        }
        if(found)break;
    }
    if(!found)return;
    auto draft=*project_->sceneEvents();
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    layout->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const QScopedValueRollback<SceneEvents *> guard(chartDraft_,&draft);
    if(dialog.exec()==QDialog::Accepted&&draft.json()!=project_->sceneEvents()->json()){
        undo_->push(new EventEdit(project_,draft.json()));changed_();
    }
}
void EventFlowEditor::editTree(QString id,QVector<int> path,int offset){
    try{QJsonObject draft;const auto trees=project_->sceneEvents()->flagEdits()[QString::number(row_)].toObject()["trees"].toArray();for(const auto &v:trees)if(v.toObject()["id"]==id)draft=v.toObject();
        if(id.isEmpty()&&list_->currentRow()>=0&&list_->currentRow()<events_.size()){const auto &selected=events_[list_->currentRow()];if(selected.group>=0&&selected.group<=5){draft["group"]=selected.group;draft["target"]=selected.triggerFields.isEmpty()?-1:selected.triggerFields.first().value;}}
        QJsonObject flags;
        if(path.isEmpty()&&offset>=0&&!id.isEmpty())for(const auto &event:events_)if(event.authoredId==id&&offset>event.entry)
            path=EventAuthoring::stepPath(draft,offset-event.entry);
        if(auto result=editEventTree(project_,row_,draft,this,&flags,path)){auto next=*project_->sceneEvents();next.apply(flags,*project_->scenePrograms());auto edited=trees;bool replaced=false;for(int n=0;n<edited.size();++n)if(edited[n].toObject()["id"]==id&&!id.isEmpty()){edited[n]=*result;replaced=true;}if(!replaced)edited<<*result;next.setTrees(row_,edited,*project_->scenePrograms());selectedAuthoredId_=(*result)["id"].toString();search_->clear();undo_->push(new EventEdit(project_,next.json()));changed_();
            for(const auto &event:events_)if(event.authoredId==(*result)["id"].toString()){selectOffset(event.entry);break;}}
    }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
}
void EventFlowEditor::editWhen(){
    if(list_->currentRow()<0||list_->currentRow()>=events_.size())return;const auto event=events_[list_->currentRow()];
    if(!canChangeTrigger(row_,event))return;
    try{auto events=*project_->sceneEvents();const auto bytes=events.program(row_);auto current=events.flagEdits()[QString::number(row_)].toObject()["triggers"].toObject()[QString::number(event.entry)].toObject();
        if(current.isEmpty()){int guard=-1;const auto test=[&](int at){return at>=0&&at+8<=bytes.size()&&u8(bytes,at)==0x61&&(u8(bytes,at+1)==0xfd||u8(bytes,at+1)==0xf8)&&u8(bytes,at+2)==0x6a&&u16(bytes,at+4)==0xa601&&u16(bytes,at+6)==0x2f00;};
            if(test(event.entry))guard=event.entry;else if(!event.triggerInstructions.isEmpty()&&test(event.triggerInstructions.first()))guard=event.triggerInstructions.first();current={{"group",event.group},{"guard",guard},{"target",guard>=0?u8(bytes,guard+3):-1}};}
        if(auto selected=editEventTrigger(project_,row_,current,this)){events.setTrigger(row_,event.entry,*selected,*project_->scenePrograms());undo_->push(new EventEdit(project_,events.json()));changed_();selectOffset(event.entry);}
    }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
}
void EventFlowEditor::editFlags(){
    auto state=*project_->sceneEvents();QDialog dialog(this);dialog.setWindowTitle("Custom flags");dialog.resize(620,450);
    auto layout=new QVBoxLayout(&dialog);auto list=new QListWidget;list->setObjectName("customFlagsList");layout->addWidget(list,1);
    auto buttons=new QHBoxLayout;layout->addLayout(buttons);auto create=new QPushButton("Create flag…"),rename=new QPushButton("Rename…"),remove=new QPushButton("Delete");
    create->setObjectName("customFlagCreate");rename->setObjectName("customFlagRename");remove->setObjectName("customFlagDelete");buttons->addWidget(create);buttons->addWidget(rename);buttons->addWidget(remove);
    auto error=label({});layout->addWidget(error);
    auto refresh=[&](int selected=-1){list->clear();for(const auto &v:state.customFlags()){const auto flag=v.toObject();if(flag["deleted"].toBool())continue;const int id=flag["id"].toInt();auto item=new QListWidgetItem(state.flagName(id),list);item->setData(Qt::UserRole,id);if(id==selected)list->setCurrentItem(item);}rename->setEnabled(list->currentItem());remove->setEnabled(list->currentItem());};
    refresh();connect(list,&QListWidget::currentRowChanged,&dialog,[&]{rename->setEnabled(list->currentItem());remove->setEnabled(list->currentItem());});
    auto editName=[&](bool existing){try{const int id=existing?list->currentItem()->data(Qt::UserRole).toInt():-1;QString initial;
        for(const auto &v:state.customFlags())if(v.toObject()["id"].toInt()==id)initial=v.toObject()["name"].toString();
        bool ok=false;const auto name=QInputDialog::getText(&dialog,existing?"Rename flag":"Create flag","Name",QLineEdit::Normal,initial,&ok);if(!ok)return;
        int selected=id;if(existing)state.renameFlag(id,name);else selected=state.createFlag(name);error->clear();refresh(selected);
    }catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}};
    connect(create,&QPushButton::clicked,&dialog,[&]{editName(false);});connect(rename,&QPushButton::clicked,&dialog,[&]{if(list->currentItem())editName(true);});
    connect(remove,&QPushButton::clicked,&dialog,[&]{if(!list->currentItem())return;try{state.removeFlag(list->currentItem()->data(Qt::UserRole).toInt());error->clear();refresh();}catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}});
    auto done=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(done);connect(done,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(done,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&state.json()!=project_->sceneEvents()->json()){undo_->push(new EventEdit(project_,state.json()));changed_();}
}
}
