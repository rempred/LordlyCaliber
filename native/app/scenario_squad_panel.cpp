#include "app/scenario_squad_panel.h"
#include "app/formation_board.h"
#include "app/squad_members_editor.h"
#include "engine/squad_members.h"
#include <QComboBox>
#include <QCompleter>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace lordly {
ScenarioSquadPanel::ScenarioSquadPanel(Project *project,
    std::function<void(const Scenarios &, const QString &)> commit, const QString &prefix, QWidget *parent)
    : QWidget(parent), project_(project), commit_(std::move(commit)), prefix_(prefix) {
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);layout->setAlignment(Qt::AlignTop);
    auto scope = new QLabel("This mission · Changes apply automatically and support Undo.");
    scope->setWordWrap(true); layout->addWidget(scope);
    board_ = new FormationBoard(project, prefix+"Cell_"); board_->setPixelScale(2);
    layout->addWidget(board_,0,Qt::AlignHCenter);
    summary_ = new QLabel; summary_->setWordWrap(true); layout->addWidget(summary_);
    edit_ = new QPushButton("Edit members and equipment…"); edit_->setObjectName(prefix+"EditMembers");
    layout->addWidget(edit_);
    connect(edit_, &QPushButton::clicked, this, &ScenarioSquadPanel::editMembers);
    connect(board_, &FormationBoard::cellMoved, this, &ScenarioSquadPanel::moveMember);
    auto form = new QFormLayout; form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto choice = [&](const QString &name, const QString &label, const QStringList &values) {
        auto box = new QComboBox; box->setObjectName(prefix+name);
        box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        box->setMinimumContentsLength(12);
        for(int i=0;i<values.size();++i) box->addItem(values[i],i);
        form->addRow(label,box);
        connect(box,&QComboBox::activated,this,[this]{changeDeployment();});
        return box;
    };
    side_=choice("Side","Side",{"Enemy","Allied"});
    facing_=choice("Facing","Facing phase",{});
    for(int i=1;i<=8;++i) facing_->addItem(QString("Direction %1").arg(i),i);
    // Direction numbers are the original eight phase values; retain any other loaded phase.
    healing_=choice("Healing","Healing supplies",{"None","Heal Leaf","2 Heal Leaves","2 Heal Leaves + Heal Seed","2 Heal Seeds","2 Heal Seeds + Heal Pack","2 Heal Packs"});
    fruit_=choice("Fruit","Fruit supplies",{"None","2 Power Fruits","Power Fruit + Angel Fruit","2 Angel Fruits"});
    revival_=choice("Revival","Revival supplies",{"None","1 Revive Stone","2 Revive Stones"});
    drop_=choice("Drop","Defeat drop",{});
    drop_->setEditable(true);drop_->setInsertPolicy(QComboBox::NoInsert);
    drop_->completer()->setCompletionMode(QCompleter::PopupCompletion);drop_->completer()->setFilterMode(Qt::MatchContains);drop_->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    connect(drop_->lineEdit(),&QLineEdit::editingFinished,this,[this]{
        if(!loading_ && drop_->currentText()!=drop_->itemText(drop_->currentIndex())) {
            refresh(key_,row_);notice_->setText("Choose a listed item to change the defeat drop. The saved value is unchanged.");
        }
    });
    for(const auto table:{Table::Consumables,Table::Items}) for(const auto &r:project_->records(table))
        drop_->addItem(QString("%1 · %2").arg(table==Table::Items?"Equipment":"Consumable",r.name),r.id|(table==Table::Items?0x8000:0));
    layout->addLayout(form);
    restore_=new QPushButton("Restore this squad from opened ROM");restore_->setObjectName(prefix+"RestoreSquad");
    layout->addWidget(restore_);
    connect(restore_,&QPushButton::clicked,this,[this]{
        try {
            auto next=*project_->scenarios();auto d=next.scenario(key_).layout.deployments[row_];
            const auto &base=next.original(key_);
            for(int row=0;row<base.squads.size();++row) if((u8(d,0)&127)==(u8(base.layout.deployments[row],0)&127)) {
                const auto identity=d.mid(1,2),tail=d.right(2);d=base.layout.deployments[row];d.replace(1,2,identity);d.replace(16,2,tail);
                next.editSquad(key_,row_,base.squads[row],d,project_->classData());commit_(next,"Restore scenario squad");return;
            }
        } catch(const Error &e) {notice_->setText(QString::fromUtf8(e.what()));}
    });
    notice_=new QLabel;notice_->setWordWrap(true);notice_->setObjectName(prefix+"SquadNotice");layout->addWidget(notice_);
}
void ScenarioSquadPanel::refresh(int key,int row) {
    loading_=true;key_=key;row_=row;setEnabled(key>=0 && row>=0);board_->clearUnits();summary_->clear();notice_->clear();
    if(key>=0 && row>=0) {
        const auto &s=project_->scenarios()->scenario(key);const auto &d=s.layout.deployments[row];
        drop_->clear();
        for(const auto table:{Table::Consumables,Table::Items})for(const auto &r:project_->records(table))
            drop_->addItem(QString("%1 · %2").arg(table==Table::Items?"Equipment":"Consumable",r.name),r.id|(table==Table::Items?0x8000:0));
        const auto members=squadMembers(s.squads[row]);QStringList names;
        for(int i=0;i<members.size();++i) {const auto &m=members[i];const auto name=QString(project_->record(Table::Classes,m.classId).name).replace('\n',' ');
            board_->setUnit(m.cell,m.classId,name);names<<QString("%1%2 · Level %3").arg(name,i?"":" (Leader)").arg(s.layout.baseLevel()+m.levelOffset);}
        summary_->setText(names.join('\n'));edit_->setEnabled(members.size()<=5);
        auto choose=[](QComboBox *box,int value){if(box->findData(value)<0)box->addItem(QString("Original value %1").arg(value),value);box->setCurrentIndex(box->findData(value));};
        choose(side_,bool(u8(d,0)&128));choose(facing_,u8(d,5));choose(healing_,u8(d,13));choose(fruit_,u8(d,14));choose(revival_,u8(d,15));choose(drop_,u16(d,11)|(u8(d,10)?0x8000:0));
        bool original=false;for(const auto &r:project_->scenarios()->original(key).layout.deployments)original|=(u8(r,0)&127)==(u8(d,0)&127);
        restore_->setEnabled(original);
    }
    board_->finishUpdate();loading_=false;
}
void ScenarioSquadPanel::changeDeployment() {
    if(loading_ || key_<0 || row_<0)return;
    try {
        auto next=*project_->scenarios();auto d=next.scenario(key_).layout.deployments[row_];
        put8(d,0,(u8(d,0)&127)|(side_->currentData().toInt()?128:0));put8(d,5,facing_->currentData().toInt());
        put8(d,13,healing_->currentData().toInt());put8(d,14,fruit_->currentData().toInt());put8(d,15,revival_->currentData().toInt());
        const int drop=drop_->currentData().toInt();
        if(drop!=(u16(d,11)|(u8(d,10)?0x8000:0))){put8(d,10,bool(drop&0x8000));put16(d,11,drop&0x7fff);}
        next.setDeployment(key_,row_,d,project_->classData());commit_(next,"Change squad deployment and supplies");
    } catch(const Error &e) {const auto message=QString::fromUtf8(e.what());refresh(key_,row_);notice_->setText(message);}
}
void ScenarioSquadPanel::moveMember(int from,int to) {
    if(loading_ || key_<0 || row_<0)return;
    try {
        auto next=*project_->scenarios();const auto &s=next.scenario(key_);auto members=squadMembers(s.squads[row_]);int moving=-1;
        for(int i=0;i<members.size();++i)if(members[i].cell==from)moving=i;
        if(moving<0)return;
        for(int i=0;i<members.size();++i)if(i!=moving && members[i].cell==to)members[i].cell=from;
        members[moving].cell=to;validateSquadFormation(members,project_->classData());
        next.editSquad(key_,row_,editSquadMembers(s.squads[row_],members),s.layout.deployments[row_],project_->classData());
        commit_(next,"Move scenario squad member");
    } catch(const Error &e) {notice_->setText(QString::fromUtf8(e.what()));board_->update();}
}
void ScenarioSquadPanel::editMembers() {
    if(key_<0 || row_<0)return;
    const int key=key_,row=row_;const auto &s=project_->scenarios()->scenario(key);
    QDialog dialog(this);dialog.setObjectName(prefix_+"MemberDialog");dialog.setWindowTitle(QString("Squad %1 — members and equipment").arg(u8(s.layout.deployments[row],0)&127));dialog.resize(850,650);
    auto layout=new QVBoxLayout(&dialog);auto help=new QLabel(QString("Mission base level: %1. Member level = base level + offset. Save applies the complete formation as one Undo step.").arg(s.layout.baseLevel()));help->setWordWrap(true);layout->addWidget(help);
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto members=new SquadMembersEditor(project_,prefix_,1,1);scroll->setWidget(members);layout->addWidget(scroll);
    members->setMembers(squadMembers(s.squads[row]));
    if(key==25 && (u8(s.layout.deployments[row],0)&127)==40)for(int i=1;i<squadMembers(s.squads[row]).size();++i)members->setLevelEditable(i,false);
    auto error=new QLabel;error->setWordWrap(true);error->setObjectName(prefix_+"MemberError");layout->addWidget(error);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&,this]{
        try {auto next=*project_->scenarios();const auto &current=next.scenario(key);
            next.editSquad(key,row,editSquadMembers(current.squads[row],members->members()),current.layout.deployments[row],project_->classData());
            commit_(next,"Edit scenario squad members");dialog.accept();
        } catch(const Error &e){error->setText(QString::fromUtf8(e.what()));}
    });
    dialog.exec();
}
}
