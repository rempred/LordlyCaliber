#include "app/squad_order_editor.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
SquadOrderEditor::SquadOrderEditor(Project *project,
    std::function<void(const Scenarios &, const QString &)> commit)
    : project_(project), commit_(std::move(commit)) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);layout->setAlignment(Qt::AlignTop);
    current_ = new QLabel;
    current_->setWordWrap(true);
    layout->addWidget(current_);
    auto form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    command_ = new QComboBox;
    command_->setObjectName("squadCommand");
    command_->addItem("Keep current scripted orders", -1);
    for (const auto &label : {"Guard this position", "Intercept nearby enemies", "Move",
                              "Move and attack", "Hit and run", "Evade", "Retreat"})
        command_->addItem(label, command_->count() - 1);
    form->addRow("Command", command_);
    destinations_ = new QWidget;
    auto destinations = new QFormLayout(destinations_);
    destinations->setContentsMargins(0, 0, 0, 0);
    destinations->setRowWrapPolicy(QFormLayout::WrapLongRows);
    destination_ = new QComboBox;
    destination_->setObjectName("squadCommandDestination");
    destinations->addRow("Destination", destination_);
    coordinates_ = new QWidget;
    auto coordinates = new QHBoxLayout(coordinates_);
    coordinates->setContentsMargins(0, 0, 0, 0);
    x_ = new QSpinBox;
    z_ = new QSpinBox;
    x_->setObjectName("squadCommandX"); z_->setObjectName("squadCommandZ");
    x_->setRange(1, 255); z_->setRange(1, 255);
    coordinates->addWidget(new QLabel("X")); coordinates->addWidget(x_);
    coordinates->addWidget(new QLabel("Z")); coordinates->addWidget(z_);
    coordinates->addStretch();
    destinations->addRow("Map position", coordinates_);
    pick_ = new QPushButton("Choose destination on map");
    pick_->setObjectName("squadCommandPick");
    pick_->setCheckable(true);
    pick_->hide();
    destinations->addRow(pick_);
    form->addRow(destinations_);
    layout->addLayout(form);
    help_ = new QLabel;
    help_->setObjectName("squadCommandHelp"); help_->setWordWrap(true);
    layout->addWidget(help_);
    apply_ = new QPushButton("Apply command to squad");
    apply_->setObjectName("squadCommandApply");
    layout->addWidget(apply_);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    notice_->setObjectName("squadCommandNotice");
    layout->addWidget(notice_);
    leaderButton_=new QPushButton("Select movement-group leader");leaderButton_->setObjectName("squadCommandLeader");layout->addWidget(leaderButton_);
    connect(leaderButton_,&QPushButton::clicked,this,[this]{if(selectLeader)selectLeader(leader_);});
    discard_=new QPushButton("Discard pending order");discard_->setObjectName("squadCommandDiscard");layout->addWidget(discard_);
    connect(discard_,&QPushButton::clicked,this,[this]{drafts_.remove(identity_);refresh(key_,squad_);});
    for(auto box:{x_,z_}) {box->setKeyboardTracking(false);connect(box,&QSpinBox::valueChanged,this,[this]{edited(true);});}
    connect(command_, &QComboBox::currentIndexChanged, this, [this] {
        if(loading_)return;cancelPick();
        if(command_->currentData().toInt()<int(SquadBehavior::Move)) ready_=true;
        else if(hasDraft() && drafts_[identity_].command<int(SquadBehavior::Move))ready_=false;
        else if(!hasDraft()) {const auto old=project_->scenarios()->squadCommand(key_,squad_);ready_=old && int(old->behavior)>=int(SquadBehavior::Move);}
        edited();
    });
    connect(destination_,&QComboBox::currentIndexChanged,this,[this]{if(!loading_){cancelPick();ready_=destination_->currentData().toInt()!=0;edited();}});
    connect(pick_, &QPushButton::toggled, this, [this](bool on) {
        pick_->setText(on ? "Cancel destination selection" : "Choose destination on map");
        if (picking_) picking_(on);
    });
    connect(apply_,&QPushButton::clicked,this,&SquadOrderEditor::applyCommand);
}
void SquadOrderEditor::enableMapPicking(std::function<void(bool)> callback) {
    picking_ = std::move(callback);
    pick_->setVisible(bool(picking_));
}
void SquadOrderEditor::cancelPick() { pick_->setChecked(false); }
void SquadOrderEditor::hideEvent(QHideEvent *event) {
    cancelPick();
    QWidget::hideEvent(event);
}
void SquadOrderEditor::setDestination(QPoint point) {
    loading_=true;destination_->setCurrentIndex(destination_->findData(0));x_->setValue(point.x());z_->setValue(point.y());loading_=false;
    cancelPick();edited(true);
}
bool SquadOrderEditor::hasDraft() const {return drafts_.contains(identity_);}
std::optional<SquadCommand> SquadOrderEditor::previewCommand() const {
    if(!hasDraft() || !ready_ || follower_ || command_->currentData().toInt()<0)return {};
    const int point=destination_->currentData().toInt();
    return SquadCommand{SquadBehavior(command_->currentData().toInt()),point?point:x_->value(),point?0:z_->value()};
}
void SquadOrderEditor::edited(bool destinationReady) {
    if(loading_ || key_<0 || squad_<0)return;
    if(destinationReady)ready_=true;
    const int action=command_->currentData().toInt();
    if(action<0){drafts_.remove(identity_);controls();return;}
    drafts_[identity_]={action,destination_->currentData().toInt(),x_->value(),z_->value(),ready_,basis_};
    if(!complex_ && ready_ && !follower_)applyCommand();else controls();
}
void SquadOrderEditor::applyCommand() {
    const auto command=previewCommand();if(!command)return;
    try {
        auto next=*project_->scenarios();next.setSquadCommand(key_,squad_,*command,project_->classData());
        drafts_.remove(identity_);commit_(next,"Change squad command");refresh(key_,squad_);
        notice_->setText("Order applied. Undo restores the previous order.");
    } catch(const Error &e){notice_->setText(QString::fromUtf8(e.what()));controls();}
}
void SquadOrderEditor::refresh(int key, int squad) {
    cancelPick();loading_=true;key_=key;squad_=squad;
    setEnabled(key>=0 && squad>=0);
    if(key<0 || squad<0){loading_=false;return;}
    const auto &scenario=project_->scenarios()->scenario(key);const auto &rows=scenario.layout.deployments;const auto &deployment=rows[squad];
    identity_={key,u8(deployment,0)&127};basis_=deployment.mid(6,1);
    for(const auto &node:scenario.layout.nodes)basis_+=node;
    for(const auto &row:rows)basis_+=row.mid(16,2);
    leader_=squad;
    if(squad>1 && u8(rows[squad-1],16))while(leader_>1 && rows[leader_-1][16]==rows[leader_-2][16])--leader_;
    follower_=leader_!=squad;
    destination_->clear();destination_->addItem("Map position",0);
    if(project_->world()) {
        const auto &world=*project_->world();const auto &sites=world.scenario(key).sites;
        for(int i=0;i<world.siteCount(key);++i)destination_->addItem(world.townName(u16(sites,1+i*4)),i+1);
    }
    auto command=project_->scenarios()->squadCommand(key,squad);
    if(!command && u8(deployment,6)==1)command=SquadCommand{SquadBehavior::Guard};
    complex_=!command.has_value();
    int action=command?int(command->behavior):-1,x=command?command->x:u8(deployment,3),z=command?command->z:u8(deployment,4);
    ready_=command && action>=int(SquadBehavior::Move);
    const bool staleDraft=drafts_.contains(identity_) && drafts_[identity_].basis!=basis_;
    if(staleDraft)drafts_.remove(identity_);
    current_->setText(command?"Current order: "+command_->itemText(command_->findData(action)):"Current order: scripted route. Original behavior is preserved until you replace it.");
    int point=z?0:x;
    if(drafts_.contains(identity_)){const auto d=drafts_[identity_];action=d.command;point=d.destination;x=d.x;z=d.z;ready_=d.ready;}
    if(point && destination_->findData(point)<0)destination_->addItem(QString("Scenario point %1").arg(point),point);
    command_->setCurrentIndex(command_->findData(action));destination_->setCurrentIndex(destination_->findData(point));
    x_->setValue(point?128:x);z_->setValue(point?128:std::max(1,z));
    loading_=false;notice_->clear();controls();
    if(staleDraft)notice_->setText("The pending order was cleared because its saved route changed. The current project order is shown.");
}
void SquadOrderEditor::controls() {
    const int action=command_->currentData().toInt();const bool moving=action>=int(SquadBehavior::Move);
    destinations_->setVisible(moving);
    static_cast<QFormLayout *>(destinations_->layout())->setRowVisible(coordinates_,destination_->currentData().toInt()==0);
    const QStringList help{
        "Stay at the starting position without seeking nearby enemies. Battles can still occur on contact.",
        "Wait here and pursue nearby enemies when the game's target checks allow it.",
        "Move to the destination without seeking nearby enemies. Battles can still occur on contact.",
        "Move to the destination and pursue nearby enemies when the game's target checks allow it.",
        "Move to the destination using Hit and Run movement and seek nearby enemies.",
        "Move to the destination using Evasion movement without seeking nearby enemies.",
        "Use the game's retreat movement toward the destination."};
    QString text=action<0?"Choose a new order, or view the existing steps in Orders. Complex scripts remain under Advanced.":help.value(action);
    if(follower_)text="This squad follows its movement-group leader. Select the leader to change the group's orders.";
    else if(hasDraft() && !ready_ && moving)text+=" Choose a destination to finish this order. This pending choice is retained when you select another object.";
    else if(hasDraft() && complex_)text+=" Destination/order selected — not applied. Replace scripted orders commits this change; Undo restores the original script.";
    else if(action>=0)text+=" Changes apply automatically. Undo restores the previous order.";
    help_->setText(text);command_->setEnabled(!follower_);destinations_->setEnabled(!follower_);
    apply_->setText("Replace scripted orders");apply_->setVisible(complex_ && action>=0 && !follower_);apply_->setEnabled(ready_);
    pick_->setEnabled(moving && !follower_);discard_->setVisible(hasDraft());leaderButton_->setVisible(follower_ && bool(selectLeader));
    if(stateChanged)stateChanged();
}
}
