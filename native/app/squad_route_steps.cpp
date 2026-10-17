#include "app/squad_route_steps.h"
#include "app/shared_impact.h"
#include "app/squad_trigger_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace lordly {
namespace {
QComboBox *combo(const char *name) {
    auto w = new QComboBox; w->setObjectName(name);
    w->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    w->setMinimumContentsLength(12);
    return w;
}
void choose(QComboBox *w, int value) {
    if (w->findData(value) < 0) w->addItem(QString("Preserved value %1").arg(value), value);
    w->setCurrentIndex(w->findData(value));
}
QLabel *label(const QString &text = {}) {
    auto w = new QLabel(text); w->setWordWrap(true); w->setTextFormat(Qt::PlainText); return w;
}
}
SquadRouteSteps::SquadRouteSteps(Project *project,
    std::function<void(const Scenarios &, const QString &)> commit, QWidget *parent)
    : QWidget(parent), project_(project), commit_(std::move(commit)) {
    setObjectName("squadRouteSteps");
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    layout->setAlignment(Qt::AlignTop);
    layout->addWidget(label("Route steps · Select a step here or its number on the map. Next step determines the order, including branches and repeats. Changes apply automatically with Undo."));
    steps_ = new QListWidget; steps_->setObjectName("squadSteps"); steps_->setWordWrap(true);
    steps_->setMinimumHeight(140); steps_->setMaximumHeight(270);
    steps_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    steps_->setSpacing(3); layout->addWidget(steps_);
    scope_ = label(); scope_->setObjectName("squadStepScope"); layout->addWidget(scope_);
    shared_ = new QCheckBox("Edit this shared route for all its squads");
    shared_->setObjectName("squadStepShared"); layout->addWidget(shared_);
    leader_ = new QPushButton("Select movement-group leader"); leader_->setObjectName("squadStepLeader");
    layout->addWidget(leader_);
    connect(leader_, &QPushButton::clicked, this, [this] { if(selectLeader) selectLeader(route_.leader); });
    details_ = new QWidget; auto form = new QFormLayout(details_);
    form->setContentsMargins(0,0,0,0); form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setSizeConstraint(QLayout::SetMinimumSize);
    heading_ = label(); heading_->setObjectName("squadStepHeading"); form->addRow(heading_);
    action_ = combo("squadStepAction");
    const QStringList actions{"Guard", "Intercept nearby enemies", "Move", "Move and attack",
                               "Hit and run", "Evade", "Retreat", "Wait dormant"};
    action_->addItem("Preserve original action", -1);
    for (int i=0;i<actions.size();++i) action_->addItem(actions[i],i);
    form->addRow("Action",action_);
    place_=new QWidget; auto places=new QVBoxLayout(place_);places->setContentsMargins(0,0,0,0);
    places->setSizeConstraint(QLayout::SetMinimumSize);
    place_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    destination_=combo("squadStepDestination");places->addWidget(destination_);
    coordinates_=new QWidget;auto coords=new QHBoxLayout(coordinates_);coords->setContentsMargins(0,0,0,0);
    x_=new QSpinBox;z_=new QSpinBox;x_->setObjectName("squadStepX");z_->setObjectName("squadStepZ");
    for(auto w:{x_,z_}){w->setRange(1,255);w->setKeyboardTracking(false);}
    coords->addWidget(new QLabel("X"));coords->addWidget(x_);coords->addWidget(new QLabel("Z"));coords->addWidget(z_);
    places->addWidget(coordinates_);
    pick_=new QPushButton("Choose destination on map");pick_->setCheckable(true);pick_->setObjectName("squadStepPick");places->addWidget(pick_);
    form->addRow("Destination",place_);
    when_=combo("squadStepWhen");
    when_->addItem("No extra trigger",0);when_->addItem("A trigger is true",1);
    when_->addItem("All triggers are true",2);when_->addItem("Any trigger is true",3);when_->addItem("Branch using triggers",4);
    when_->addItem("Preserved script (Advanced)",-1);form->addRow("Continue when",when_);
    conditions_=new QWidget;auto conditionForm=new QVBoxLayout(conditions_);conditionForm->setContentsMargins(0,0,0,0);
    conditionForm->setSizeConstraint(QLayout::SetMinimumSize);
    for(int slot=0;slot<2;++slot) {
        const auto suffix=QString(slot?"B":"A");
        auto group=new QGroupBox("Trigger "+suffix);auto content=new QVBoxLayout(group);
        triggerGroups_[slot]=group;triggers_[slot]=new SquadTriggerEditor(suffix);content->addWidget(triggers_[slot]);conditionForm->addWidget(group);
        triggers_[slot]->selected=[this,slot](int id) {
            const int row=selectedNode();
            change("Choose route trigger",[&](Scenarios &s){
                auto n=s.scenario(key_).layout.nodes[row];
                if(u8(n,11)==2 && u8(n,slot?10:12)==id && u8(s.scenario(key_).layout.conditions[id-1],1)==9)
                    s.setStepTrigger(key_,row,slot,s.scenario(key_).layout.conditions[id-1],project_->classData(),true);
                else {put8(n,slot?12:10,quint8(id));s.setNode(key_,row,n,project_->classData());}
            });
        };
        triggers_[slot]->create=[this,slot](int kind,bool create){writeTrigger(slot,defaultTrigger(kind),create);};
        triggers_[slot]->changed=[this,slot](const QByteArray &c){writeTrigger(slot,c);};
        triggers_[slot]->picking=[this,slot](bool on) {
            cancelPick();
            if(on){areaSlot_=slot;triggers_[slot]->setPicking(true);if(picking)picking(true);}
        };
    }
    conditionA_=triggers_[0]->findChild<QComboBox *>("squadTriggerAExisting");conditionA_->setObjectName("squadStepConditionA");
    conditionB_=triggers_[1]->findChild<QComboBox *>("squadTriggerBExisting");conditionB_->setObjectName("squadStepConditionB");
    form->addRow(conditions_);
    next_=combo("squadStepNext");form->addRow("Next step",next_);
    branch_=new QWidget;auto branchForm=new QFormLayout(branch_);branchForm->setContentsMargins(0,0,0,0);branchForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    fork_=combo("squadStepFork");branchForm->addRow("If A is true",fork_);form->addRow(branch_);
    form->addRow(label("With a branch, A uses its own next step; otherwise B uses Next step. If neither is true, the squad waits. Game readiness and nearby enemies still affect movement."));
    layout->addWidget(details_);
    add_=new QPushButton("Add follow-up step");add_->setObjectName("squadStepAdd");layout->addWidget(add_);
    advanced_=new QPushButton("Advanced step and trigger details…");advanced_->setObjectName("squadStepAdvanced");layout->addWidget(advanced_);
    notice_=label();notice_->setObjectName("squadStepNotice");layout->addWidget(notice_);
    connect(steps_,&QListWidget::currentRowChanged,this,[this]{if(!loading_){cancelPick();showStep();}});
    connect(shared_,&QCheckBox::toggled,this,[this]{if(!loading_){cancelPick();showStep();}});
    connect(pick_,&QPushButton::toggled,this,[this](bool on){
        if(on){areaSlot_=-1;areaCorner_.reset();for(auto t:triggers_)t->setPicking(false);}
        pick_->setText(on?"Cancel destination selection":"Choose destination on map");if(picking)picking(on);
        if(stateChanged)stateChanged();
    });
    connect(advanced_,&QPushButton::clicked,this,[this]{if(advanced)advanced(selectedNode());});
    connect(add_,&QPushButton::clicked,this,&SquadRouteSteps::addStep);
    connect(action_,&QComboBox::activated,this,[this]{
        const int action=action_->currentData().toInt();if(action<0)return;
        edit("Change route step action",[&](QByteArray &n){
            const bool moving=action>=2 && action<=6;
            // Only action/destination fields are replaced. Gates and graph edges survive.
            put8(n,1,action==7?2:moving?1:0);
            put8(n,2,action==4?1:action==5?2:0);
            put8(n,3,action==6?2:(action==1 || action==3 || action==4)?1:0);
            if(moving && !u8(n,4)) {
                const auto &d=project_->scenarios()->scenario(key_).layout.deployments[route_.leader];
                put8(n,4,u8(d,3)?u8(d,3):128);put8(n,5,u8(d,3)?u8(d,4):128);
            }
        });
    });
    connect(destination_,&QComboBox::activated,this,[this]{
        const int point=destination_->currentData().toInt();
        if(!point){coordinates_->show();pick_->setChecked(true);return;}
        edit("Change route step destination",[&](QByteArray &n){put8(n,4,quint8(point));put8(n,5,0);});
    });
    for(auto w:{x_,z_})connect(w,&QSpinBox::valueChanged,this,[this]{
        if(loading_)return;
        const int x=x_->value(),z=z_->value();edit("Move route step",[&](QByteArray &n){put8(n,4,quint8(x));put8(n,5,quint8(z));});
    });
    connect(when_,&QComboBox::activated,this,&SquadRouteSteps::transition);
    for(auto w:{next_,fork_})connect(w,&QComboBox::activated,this,[this,w]{
        const int value=w->currentData().toInt();edit("Change route follow-up",[&](QByteArray &n){put8(n,w==next_?17:16,quint8(value));});
    });
}
int SquadRouteSteps::selectedNode() const {return steps_->currentItem()?steps_->currentItem()->data(Qt::UserRole).toInt():-1;}
bool SquadRouteSteps::isPicking() const {return pick_->isChecked() || isPickingArea();}
QString SquadRouteSteps::pickPrompt() const {
    if(isPickingArea())return QString("Click the %1 corner of trigger %2's area. Escape cancels; the squad stays selected.")
        .arg(areaCorner_?"second":"first",areaSlot_?"B":"A");
    return "Click a destination for this squad. Escape cancels. The squad stays selected.";
}
void SquadRouteSteps::cancelPick(){
    const bool area=isPickingArea();areaSlot_=-1;areaCorner_.reset();
    pick_->setChecked(false);for(auto t:triggers_)t->setPicking(false);
    if(area && picking)picking(false);
    if(area && stateChanged)stateChanged();
}
void SquadRouteSteps::hideEvent(QHideEvent *e){cancelPick();QWidget::hideEvent(e);}
QString SquadRouteSteps::destination(int x,int z) const {
    if(!x)return "No destination";
    if(z)return QString("Map position %1, %2").arg(x).arg(z);
    if(project_->world() && x<=project_->world()->siteCount(key_))
        return project_->world()->townName(u16(project_->world()->scenario(key_).sites,1+(x-1)*4));
    return QString("Scenario point %1").arg(x);
}
QString SquadRouteSteps::nextName(int id) const {
    if(!id)return "Stay on this step";
    if(id==1)return "Hold position";
    if(id==255)return "Remove squad from field";
    const int number=route_.number(id);
    if(number)return QString("Step %1").arg(number);
    const auto &nodes=project_->scenarios()->scenario(key_).layout.nodes;
    if(id>=4 && id<nodes.size()+4)return QString("Other route · %1 (order %2)").arg(SquadRoute::actionName(nodes[id-4])).arg(id);
    return QString("Unresolved order %1 (preserved)").arg(id);
}
QString SquadRouteSteps::continueName(const QByteArray &n) const {
    if(!u8(n,17))return "No follow-up";
    if(SquadRoute::extended(n,&project_->scenarios()->scenario(key_).layout))return "Preserved script · Advanced";
    const auto &e=project_->scenarios()->scenario(key_).layout;
    const auto a=SquadRoute::conditionName(e,u8(n,10)),b=SquadRoute::conditionName(e,u8(n,12));
    switch(u8(n,11)){
    case 1:return "A: "+a+"; otherwise B: "+b;
    case 2:return a+" AND "+b;
    case 3:return a+" OR "+b;
    default:return a;
    }
}
void SquadRouteSteps::refresh(int key,int squad) {
    const QScopedValueRollback<bool> guard(loading_,true);cancelPick();
    if(key!=key_ || squad!=squad_)shared_->setChecked(false);
    key_=key;squad_=squad;steps_->clear();route_={};notice_->clear();
    if(key<0 || squad<0){setVisible(false);return;}
    const auto &s=project_->scenarios()->scenario(key);route_=SquadRoute::read(s,squad);
    setVisible(!route_.rows.isEmpty());
    for(int row:route_.rows){
        const auto &n=s.layout.nodes[row];QStringList places;
        if(u8(n,1)==1)for(int at:{4,6,8}){if(!u8(n,at))break;places<<destination(u8(n,at),u8(n,at+1));}
        if(places.isEmpty())places<<(u8(n,1)==2?"Starting / current position":"Where the previous step finishes");
        const int number=route_.number(row+4);
        auto follow=SquadRoute::extended(n,&s.layout)?QString("Preserved script · Advanced"):nextName(u8(n,17));
        if(u8(n,11)==1 && !SquadRoute::extended(n,&s.layout) && u8(n,17))follow="A → "+nextName(u8(n,16))+"; B → "+follow;
        auto item=new QListWidgetItem(QString("%1. Action: %2\nDestination: %3\nContinue when: %4\nNext step: %5")
            .arg(number).arg(SquadRoute::actionName(n),places.join(" → "),continueName(n),follow),steps_);
        item->setData(Qt::UserRole,row);item->setToolTip(QString("Native order %1. Branches and repeats use the displayed step numbers.").arg(row+4));
    }
    const int saved=selections_.value({key_,u8(s.layout.deployments[squad_],0)&127},route_.rows.value(0,-1));
    steps_->setCurrentRow(std::max(0,int(route_.rows.indexOf(saved))));
    showStep();
}
void SquadRouteSteps::selectNode(int row){
    const int index=route_.rows.indexOf(row);
    if(index>=0){
        steps_->setCurrentRow(index);steps_->scrollToItem(steps_->currentItem());
        for(auto ancestor=parentWidget();ancestor;ancestor=ancestor->parentWidget())
            if(auto scroll=qobject_cast<QScrollArea *>(ancestor)){scroll->ensureWidgetVisible(heading_);break;}
    }
}
void SquadRouteSteps::showStep(){
    const QScopedValueRollback<bool> guard(loading_,true);
    const int row=selectedNode();if(row<0)return;
    const auto &s=project_->scenarios()->scenario(key_);const auto &n=s.layout.nodes[row];
    selections_[{key_,u8(s.layout.deployments[squad_],0)&127}]=row;
    const bool follower=route_.leader!=squad_, shared=route_.sharedRows.contains(row),extended=SquadRoute::extended(n,&s.layout);
    const bool writable=!follower && (!shared || shared_->isChecked());
    details_->setEnabled(writable);shared_->setVisible(!follower && !route_.sharedRows.isEmpty());leader_->setVisible(follower);
    const QString explanation = follower?"This squad follows its movement-group leader. The leader's steps are shown."
        :shared?"This step may be shared with another squad or script. Enable shared editing to change it for every user."
        :"Editing this squad's route. Movement-group followers use the same orders.";
    showEditImpact(scope_, "Squads using this step", findProjectUses(*project_, "routeStep", key_, QString::number(row)), explanation);
    heading_->setText(QString("Edit step %1").arg(route_.number(row+4)));
    choose(action_,SquadRoute::action(n));
    destination_->clear();destination_->addItem("Map position (choose on map)",0);
    int pointCount=project_->world()?project_->world()->siteCount(key_):0;
    if(project_->mapEdits()) {
        try {pointCount=std::max(pointCount,int(project_->mapEdits()->points(key_).size()));}
        catch(const Error &) {} // Internal event keys can have orders without an editable battlefield.
    }
    for(int i=0;i<pointCount;++i)destination_->addItem(destination(i+1,0),i+1);
    choose(destination_,u8(n,5)?0:u8(n,4));x_->setValue(u8(n,5)?u8(n,4):128);z_->setValue(u8(n,5)?u8(n,5):128);
    place_->setEnabled(u8(n,1)==1);coordinates_->setVisible(!destination_->currentData().toInt());pick_->setVisible(bool(picking));
    for(int slot=0;slot<2;++slot) {
        const int id=u8(n,slot?12:10);int uses=0;
        for(const auto &other:s.layout.nodes)for(int at:{10,12,14})if(id && u8(other,at)==id)++uses;
        triggers_[slot]->refresh(s.layout,id,id>project_->scenarios()->original(key_).layout.conditions.size() && uses==1);
    }
    choose(when_,extended?-1:u8(n,11)==1?4:u8(n,11)?u8(n,11):u8(n,10)?1:0);
    const int mode=when_->currentData().toInt();conditions_->setVisible(mode>0);
    triggerGroups_[1]->setVisible(mode>=2);
    when_->setEnabled(!extended);conditions_->setEnabled(!extended);
    for(auto w:{next_,fork_}){w->clear();for(int id:{0,1,255})w->addItem(nextName(id),id);for(int i=0;i<s.layout.nodes.size();++i)w->addItem(nextName(i+4),i+4);}
    choose(next_,u8(n,17));choose(fork_,u8(n,16));next_->setEnabled(!extended);branch_->setVisible(mode==4);
    if(mode==4)next_->setItemText(next_->findData(0),"No follow-up (disables both branches)");
    add_->setEnabled(writable && !extended && u8(n,11)!=1 && s.layout.nodes.size()<16);
    add_->setToolTip("Inserts a Guard step after the selected step. Its previous follow-up is retained. A mission supports 16 native orders.");
    QStringList notes;
    if(extended)notes<<"This step uses an extended script. Its continuation is preserved; open Advanced to inspect it.";
    if(u8(n,6)||u8(n,8))notes<<"This order has several destinations. The destination controls edit the first; the remaining stops are preserved and numbered on the map.";
    if(!u8(n,17) && (u8(n,10)||u8(n,11)))notes<<"The selected conditions are inactive while Next step is Stay on this step.";
    if(s.layout.nodes.size()>=16)notes<<"All 16 order slots are in use. Existing steps can still be edited.";
    notes<<QString("Mission triggers: %1 / 16. Reuse a trigger from the list, or choose New to create one.").arg(s.layout.conditions.size());
    notice_->setText(notes.join('\n'));
    if(stateChanged)stateChanged();
}
void SquadRouteSteps::change(const QString &title,const std::function<void(Scenarios &)> &fn){
    if(loading_ || selectedNode()<0 || !details_->isEnabled())return;
    cancelPick();
    try{auto next=*project_->scenarios();fn(next);if(next.json()!=project_->scenarios()->json())commit_(next,title);refresh(key_,squad_);}
    catch(const Error &e){refresh(key_,squad_);notice_->setText(QString::fromUtf8(e.what()));}
}
void SquadRouteSteps::edit(const QString &title,const std::function<void(QByteArray &)> &fn){
    const int row=selectedNode();change(title,[&](Scenarios &s){auto n=s.scenario(key_).layout.nodes[row];fn(n);s.setNode(key_,row,n,project_->classData());});
}
void SquadRouteSteps::transition(){
    if(loading_)return;
    const int mode=when_->currentData().toInt();if(mode<0)return;
    const int row=selectedNode();
    change("Change route continuation",[&](Scenarios &s){
        auto n=s.scenario(key_).layout.nodes[row];
        // Reuse an available mission trigger when enabling a gate; empty missions
        // receive a real, editable area predicate in the same atomic undo entry.
        for(int slot=0;slot<(mode>=2?2:mode?1:0);++slot) {
            const int offset=slot?12:10;
            if(u8(n,offset)>0 && u8(n,offset)<=s.scenario(key_).layout.conditions.size())continue;
            int id=0;const auto &conditions=s.scenario(key_).layout.conditions;
            for(int i=0;i<conditions.size();++i)if(u8(conditions[i],1)!=14 && u8(conditions[i],1)!=20){id=i+1;break;}
            if(!id){const auto c=defaultTrigger(16);s.setStepTrigger(key_,row,slot,c,project_->classData(),true);id=u8(s.scenario(key_).layout.nodes[row],offset);}
            put8(n,offset,quint8(id));
        }
        if(!mode)put8(n,10,0);
        put8(n,11,quint8(mode==4?1:mode>=2?mode:0));
        if(mode==4 && !u8(n,16))put8(n,16,u8(n,17)?u8(n,17):1);
        s.setNode(key_,row,n,project_->classData());
        // Count predicates consume their success bit. Testing one ID twice for
        // AND can never succeed; give the second check its own native slot.
        if(mode==2 && u8(n,10)==u8(n,12) && u8(s.scenario(key_).layout.conditions[u8(n,10)-1],1)==9)
            s.setStepTrigger(key_,row,1,s.scenario(key_).layout.conditions[u8(n,10)-1],project_->classData(),true);
    });
}
void SquadRouteSteps::setDestination(QPoint p){
    if(isPickingArea()) {
        if(p.x()<0 || p.x()>255 || p.y()<0 || p.y()>255)return;
        if(!areaCorner_){areaCorner_=p;triggers_[areaSlot_]->setPicking(true,true);if(picking)picking(true);if(stateChanged)stateChanged();return;}
        const int slot=areaSlot_;const QRect area=QRect(*areaCorner_,p).normalized();
        auto c=project_->scenarios()->scenario(key_).layout.conditions[u8(project_->scenarios()->scenario(key_).layout.nodes[selectedNode()],slot?12:10)-1];
        put8(c,2,quint8(area.left()));put8(c,3,quint8(area.top()));put8(c,4,quint8(area.right()));put8(c,5,quint8(area.bottom()));
        writeTrigger(slot,c);return;
    }
    if(p.x()<1 || p.x()>255 || p.y()<1 || p.y()>255)return;
    edit("Move route step",[&](QByteArray &n){put8(n,4,quint8(p.x()));put8(n,5,quint8(p.y()));});
}
QByteArray SquadRouteSteps::defaultTrigger(int kind) const {
    QByteArray c(10,0);put8(c,1,quint8(kind));
    if(kind==9){put8(c,6,5);return c;}
    const auto &n=project_->scenarios()->scenario(key_).layout.nodes[selectedNode()];
    const int x=u8(n,5)?u8(n,4):128,z=u8(n,5)?u8(n,5):128;
    put8(c,2,quint8(std::max(0,x-8)));put8(c,3,quint8(std::max(0,z-8)));
    put8(c,4,quint8(std::min(255,x+8)));put8(c,5,quint8(std::min(255,z+8)));return c;
}
void SquadRouteSteps::writeTrigger(int slot,const QByteArray &c,bool create){
    const int row=selectedNode();change("Edit route trigger",[&](Scenarios &s){s.setStepTrigger(key_,row,slot,c,project_->classData(),create);});
}
QVector<QPair<QString,QRectF>> SquadRouteSteps::triggerAreas() const {
    QVector<QPair<QString,QRectF>> areas;
    if(key_<0 || selectedNode()<0 || !isVisible())return areas;
    const auto &e=project_->scenarios()->scenario(key_).layout;
    if(selectedNode()>=e.nodes.size())return areas;
    const auto &n=e.nodes[selectedNode()];
    if(SquadRoute::extended(n,&e))return areas;
    for(int slot=0;slot<(u8(n,11)?2:u8(n,10)?1:0);++slot) {
        const int id=u8(n,slot?12:10);if(id<1 || id>e.conditions.size())continue;
        const auto &c=e.conditions[id-1];if(u8(c,1)!=16)continue;
        areas.append({QString("Trigger %1").arg(slot?"B":"A"),QRectF(u8(c,2),u8(c,3),u8(c,4)-u8(c,2)+1,u8(c,5)-u8(c,3)+1)});
    }
    if(areaCorner_)areas.append({"First corner",QRectF(*areaCorner_,QSizeF(1,1))});
    return areas;
}
void SquadRouteSteps::addStep(){
    const int row=selectedNode();
    change("Add route follow-up",[&](Scenarios &s){
        auto n=s.scenario(key_).layout.nodes[row];
        if(SquadRoute::extended(n,&s.scenario(key_).layout)||u8(n,11)==1)throw Error("Choose the branch's destination step to add its follow-up.");
        const int added=s.addNode(key_,0,project_->classData());
        auto fresh=s.scenario(key_).layout.nodes[added];put8(fresh,17,u8(n,17));s.setNode(key_,added,fresh,project_->classData());
        put8(n,17,quint8(added+4));s.setNode(key_,row,n,project_->classData());
        selections_[{key_,u8(s.scenario(key_).layout.deployments[squad_],0)&127}]=added;
    });
}
}
