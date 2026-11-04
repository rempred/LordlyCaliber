#include "app/event_flow_editor.h"
#include "app/event_tree_dialog.h"
#include "app/event_flag_links.h"
#include "engine/event_authoring.h"
#include "engine/event_tree.h"
#include <QComboBox>
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
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QMessageBox>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class EventEdit final:public QUndoCommand {
    Project *project_;QJsonObject before_,after_;
  public:
    EventEdit(Project *p,QJsonObject after):QUndoCommand("Edit story event"),project_(p),before_(p->sceneEvents()->json()),after_(after){}
    void undo()override{project_->setSceneEvents(before_);}void redo()override{project_->setSceneEvents(after_);}
};
QLabel *label(const QString &text,QWidget *parent=nullptr){auto result=new QLabel(text,parent);result->setWordWrap(true);return result;}
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
        selectedAuthoredId_=list_->currentRow()>=0&&list_->currentRow()<events_.size()?events_[list_->currentRow()].authoredId:QString{};showEvent();});
}
void EventFlowEditor::setRow(int row,int entryFilter) {
    const int entry=row_==row&&list_->currentRow()>=0&&list_->currentRow()<events_.size()?events_[list_->currentRow()].entry:-1;
    if(row_!=row||entryFilter_!=entryFilter){focusOffset_=-1;revealedEntry_=-1;if(row_!=row)selectedAuthoredId_.clear();}
    entryFilter_=entryFilter;
    if(row_!=row)page_=0;row_=row;events_.clear();notice_->clear();const QSignalBlocker block(list_);list_->clear();
    findChild<QPushButton *>("storyCreateEvent")->setEnabled(EventAuthoring::supported(row));
    try {
        if(row>=0) {
            const bool originalScenes=!project_->scenePrograms()->modified()&&(!project_->dialogues()||!project_->dialogues()->modified())&&
                sha256(project_->rom().bytes())=="571e83396bc81e70da4c0a20313d82dbd7dfe685f2c37418c8e27f927e2cc67a";
            events_=EventFlow::read(project_->rom(),*project_->sceneEvents(),*project_->scenePrograms(),row,originalScenes);
        }
        int selected=0;for(int n=0;n<events_.size();++n){const auto &event=events_[n];
            auto item=new QListWidgetItem(event.title+"\n"+event.trigger,list_);item->setData(Qt::UserRole,event.entry);
            QString search=event.title+" "+event.trigger;for(const auto &action:event.actions)search+=" "+action.title;
            for(const auto &condition:event.requirements)search+=" "+condition.title;
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
    for(const auto &event:events_){if(event.entry==offset||event.triggerInstructions.contains(offset))return event.title;
        for(const auto &condition:event.requirements)if(condition.instructions.contains(offset))return condition.title;
        for(const auto &action:event.actions)if(action.instructions.contains(offset))return action.title;}
    return QString("Instruction @%1").arg(offset,4,16,QChar('0'));
}
void EventFlowEditor::selectOffset(int offset){
    for(int n=0;n<events_.size();++n){const auto &event=events_[n];bool found=event.entry==offset||event.triggerInstructions.contains(offset);
        if(!event.authoredId.isEmpty()&&offset>=event.entry&&(n+1>=events_.size()||offset<events_[n+1].entry))found=true;
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
    if(field.key=="destination")for(auto i=field.choices.begin();i!=field.choices.end();++i)i.value()=destination(i.key());
    const auto object=QString("storyValue_%1_%2").arg(field.offset).arg(field.key.isEmpty()?"state":field.key)+(field.mode==StoryField::SquadState?QString("_bit%1").arg(field.mask):QString{});
    if(field.choices.isEmpty()){
        auto spin=new QSpinBox;spin->setObjectName(object);spin->setRange(field.minimum,field.maximum);spin->setValue(field.value);spin->setKeyboardTracking(false);form->addRow(field.label,spin);
        connect(spin,&QSpinBox::valueChanged,this,[this,field](int value){change(field,value);});
    }else{
        auto combo=new QComboBox;combo->setObjectName(object);combo->setEditable(true);combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setMinimumContentsLength(25);combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        for(auto i=field.choices.begin();i!=field.choices.end();++i)if(i.key()>=field.minimum&&i.key()<=field.maximum)combo->addItem(i.value()+(field.kind=="named-state"?QString{}:QString(" [%1]").arg(i.key())),i.key());
        if(combo->findData(field.value)<0)combo->addItem(QString("Existing ID %1").arg(field.value),field.value);
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
        for(const auto &field:condition.fields)addField(form,field);
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
        auto preview=new QTreeWidget;preview->setColumnCount(2);preview->setHeaderHidden(true);preview->setWordWrap(true);layout->addWidget(preview);
        preview->header()->setSectionResizeMode(0,QHeaderView::Stretch);preview->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        std::function<void(QTreeWidgetItem *,const QJsonObject &)> showCondition=[&](QTreeWidgetItem *parent,const QJsonObject &condition){
            const auto type=condition["type"].toString();auto item=new QTreeWidgetItem(parent,{type=="all"?"ALL OF (AND)":type=="any"?"ANY OF (OR)":type=="not"?"NOT":eventConditionSummary(condition,row_,&*project_->sceneEvents())});
            if(type=="all"||type=="any")for(const auto &v:condition["conditions"].toArray())showCondition(item,v.toObject());
            else if(type=="not")showCondition(item,condition["condition"].toObject());
            else if(type=="flag")preview->setItemWidget(item,1,eventFlagLinks(project_,row_,condition["flag"].toInt(),[this](int row,int at){if(openEventInstruction)openEventInstruction(row,at);else if(row==row_)selectOffset(at);}));
        };
        std::function<void(QTreeWidgetItem *,QJsonArray)> show=[&](QTreeWidgetItem *parent,QJsonArray nodes){for(const auto &v:nodes){auto n=v.toObject();auto item=new QTreeWidgetItem(parent,{eventTreeSummary(n,row_,&*project_->sceneEvents())});if(n["type"]=="if"){
            showCondition(item,n["condition"].toObject());
            show(new QTreeWidgetItem(item,{"THEN"}),n["then"].toArray());show(new QTreeWidgetItem(item,{"ELSE"}),n["else"].toArray());}}};show(preview->invisibleRootItem(),tree["nodes"].toArray());preview->expandAll();layout->addStretch();return;
    }
    if(EventAuthoring::supported(row_)&&row_!=0&&row_!=70){auto change=new QPushButton("Change WHEN…");change->setObjectName("storyChangeWhen");triggerForm->addRow(change);connect(change,&QPushButton::clicked,this,[this]{editWhen();});}
    for(const auto &f:event.triggerFields)addField(triggerForm,f);
    if(!event.requirements.isEmpty()){
        auto guard=new QGroupBox("IF");auto guardForm=new QFormLayout(guard);StoryCondition requirements;
        requirements.operation="all";
        for(const auto &requirement:event.requirements){
            auto condition=requirement.condition;
            if(condition.operation.isEmpty()){condition.operation="test";condition.title=requirement.title;condition.fields=requirement.fields;}
            requirements.children<<condition;
        }
        addCondition(guardForm,requirements.children.size()==1?requirements.children.first():requirements);
        guard->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);triggerForm->addRow(guard);
    }
    when->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);layout->addWidget(when);
    if(focusOffset_>=0&&focusOffset_!=event.entry){auto back=new QPushButton("Back to event start");back->setObjectName("storyPreviousSteps");
        layout->addWidget(back);connect(back,&QPushButton::clicked,this,[this]{focusOffset_=-1;showEvent();});}
    const auto tree=storyTree(event,project_->sceneEvents()->program(row_),focusOffset_);
    std::function<void(QVBoxLayout *,const QVector<StoryTreeNode> &)> render;
    bool hasNext=false;
    render=[&](QVBoxLayout *parent,const QVector<StoryTreeNode> &nodes){
        for(const auto &node:nodes){
            if(node.kind==StoryTreeNode::Reference){
                auto link=new QPushButton(node.referenceLabel+" · "+destination(node.offset));
                link->setEnabled(node.offset>=0);parent->addWidget(link);
                if(node.continuation&&!hasNext){link->setObjectName("storyNextSteps");hasNext=true;}
                connect(link,&QPushButton::clicked,this,[this,at=node.offset]{selectOffset(at);});continue;
            }
            const auto &action=event.actions[node.action];
            auto box=new QGroupBox(node.kind==StoryTreeNode::Decision?"IF":action.kind=="requirement"?"IF":"THEN");
            box->setObjectName(QString("storyAction_%1").arg(node.offset));
            auto form=new QFormLayout(box);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
            if(node.kind==StoryTreeNode::Decision)addCondition(form,node.condition);
            else{
                if(!action.condition.operation.isEmpty())addCondition(form,action.condition);
                else form->addRow(label(action.title));
                for(const auto &f:action.fields)addField(form,f);
                if(!action.detail.isEmpty())form->addRow(label(action.detail));
            }
            if(node.kind==StoryTreeNode::Decision){
                auto branches=new QVBoxLayout;
                const auto branch=[&](const QString &title,const QVector<StoryTreeNode> &body){
                    auto group=new QGroupBox(title);group->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
                    auto nested=new QVBoxLayout(group);
                    if(body.isEmpty())nested->addWidget(label("Continue below."));else render(nested,body);
                    branches->addWidget(group);
                };
                branch("THEN",node.thenBranch);branch("ELSE",node.elseBranch);form->addRow(branches);
            }else{
                auto links=new QHBoxLayout;
                if(action.kind=="program"&&!action.fields.isEmpty()){
                    auto open=new QPushButton("Open event program");open->setObjectName(QString("storyOpenProgram_%1").arg(action.fields.first().offset));
                    open->setEnabled(bool(openEventProgram));links->addWidget(open);
                    connect(open,&QPushButton::clicked,this,[this,row=action.fields.first().value]{if(openEventProgram)openEventProgram(row);});}
                if(action.scene>=0){auto edit=new QPushButton("Edit cutscene…");edit->setEnabled(bool(editCutscene));links->addWidget(edit);
                    connect(edit,&QPushButton::clicked,this,[this,id=action.scene]{if(editCutscene)editCutscene(id);});}
                auto route=[&](const QString &prefix,int at){auto target=label("<a href=\"route\">"+(prefix+destination(at)).toHtmlEscaped()+"</a>");
                    form->addRow(target);connect(target,&QLabel::linkActivated,this,[this,at]{selectOffset(at);});};
                if(action.target>=0)route(action.otherwise>=0?"Condition met → ":"Go to → ",action.target);
                if(action.otherwise>=0)route("Otherwise → ",action.otherwise);
                if(action.advanced||!action.detail.isEmpty()){
                    auto raw=new QPushButton("Raw instructions…");links->addWidget(raw);
                    connect(raw,&QPushButton::clicked,this,[this,at=node.offset]{if(rawInstructions)rawInstructions(at);});}
                if(links->count())form->addRow(links);else delete links;
            }
            box->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);parent->addWidget(box);
        }
    };
    render(layout,tree.nodes);
    if(!tree.otherEntries.isEmpty()){
        auto other=new QHBoxLayout;auto choices=new QComboBox;choices->setObjectName("storyOtherPaths");
        choices->setMinimumContentsLength(25);choices->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        for(int at:tree.otherEntries)choices->addItem(destination(at),at);
        auto open=new QPushButton("Open path");other->addWidget(new QLabel("Other script paths"));other->addWidget(choices,1);other->addWidget(open);
        connect(open,&QPushButton::clicked,this,[this,choices]{selectOffset(choices->currentData().toInt());});layout->addLayout(other);
    }
    auto raw=new QPushButton("View this event’s raw instructions");layout->addWidget(raw);connect(raw,&QPushButton::clicked,this,[this,at=event.entry]{if(rawInstructions)rawInstructions(at);});layout->addStretch();
}
void EventFlowEditor::change(StoryField field,int value){
    const int row=row_;QTimer::singleShot(0,this,[this,row,field,value]{
        try{auto next=*project_->sceneEvents();EventFlow::set(next,*project_->scenePrograms(),row,field,value);
            if(next.json()!=project_->sceneEvents()->json())undo_->push(new EventEdit(project_,next.json()));
            changed_();
        }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
    });
}
void EventFlowEditor::editTree(QString id){
    try{QJsonObject draft;const auto trees=project_->sceneEvents()->flagEdits()[QString::number(row_)].toObject()["trees"].toArray();for(const auto &v:trees)if(v.toObject()["id"]==id)draft=v.toObject();
        if(id.isEmpty()&&list_->currentRow()>=0&&list_->currentRow()<events_.size()){const auto &selected=events_[list_->currentRow()];draft["group"]=selected.group;draft["target"]=selected.triggerFields.isEmpty()?-1:selected.triggerFields.first().value;}
        QJsonObject flags;
        if(auto result=editEventTree(project_,row_,draft,this,&flags)){auto next=*project_->sceneEvents();next.apply(flags,*project_->scenePrograms());auto edited=trees;bool replaced=false;for(int n=0;n<edited.size();++n)if(edited[n].toObject()["id"]==id&&!id.isEmpty()){edited[n]=*result;replaced=true;}if(!replaced)edited<<*result;next.setTrees(row_,edited,*project_->scenePrograms());selectedAuthoredId_=(*result)["id"].toString();search_->clear();undo_->push(new EventEdit(project_,next.json()));changed_();
            for(const auto &event:events_)if(event.authoredId==(*result)["id"].toString()){selectOffset(event.entry);break;}}
    }catch(const std::exception &e){notice_->setText(QString::fromUtf8(e.what()));}
}
void EventFlowEditor::editWhen(){
    if(list_->currentRow()<0||list_->currentRow()>=events_.size())return;const auto event=events_[list_->currentRow()];
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
