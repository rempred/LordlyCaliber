#include "app/leader_affinity_editor.h"
#include "app/content_refresh.h"
#include <QGridLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>

namespace lordly {
namespace {
class AffinityCommand final : public GameRuleUndoCommand {
  public:
    AffinityCommand(Project *p, QJsonObject after)
        : GameRuleUndoCommand("Change combat leader affinity"), project_(p),
          before_(p->tools()->json()), after_(std::move(after)) {}
    void undo() override { project_->setTools(before_); }
    void redo() override { project_->setTools(after_); }
  private:
    Project *project_;
    QJsonObject before_, after_;
};
QLabel *wrapped(const QString &text = {}) {
    auto label = new QLabel(text); label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText); return label;
}
}
namespace {
std::optional<LeaderAffinity> manageGroups(Project *project,const LeaderAffinity &original,QWidget *parent) {
    auto next=original;QDialog dialog(parent);dialog.setObjectName("combatAffinityGroups");
    dialog.setWindowTitle("Combat affinity groups");dialog.resize(1000,650);
    auto root=new QVBoxLayout(&dialog);auto body=new QHBoxLayout;root->addLayout(body,1);
    auto groupPanel=new QVBoxLayout;body->addLayout(groupPanel,1);
    groupPanel->addWidget(new QLabel("Groups"));auto groups=new QListWidget;
    groups->setObjectName("combatAffinityGroupsList");groupPanel->addWidget(groups,1);
    auto groupActions=new QHBoxLayout;groupPanel->addLayout(groupActions);
    auto add=new QPushButton("Add…"),rename=new QPushButton("Rename…"),remove=new QPushButton("Delete");
    add->setObjectName("combatAffinityAddGroup");rename->setObjectName("combatAffinityRenameGroup");remove->setObjectName("combatAffinityDeleteGroup");
    for(auto button:{add,rename,remove})groupActions->addWidget(button);
    auto memberPanel=new QVBoxLayout;body->addLayout(memberPanel,2);memberPanel->addWidget(new QLabel("Member classes"));
    auto memberSearch=new QLineEdit;memberSearch->setPlaceholderText("Find a member class…");memberPanel->addWidget(memberSearch);
    auto members=new QListWidget;members->setObjectName("combatAffinityMembers");memberPanel->addWidget(members,1);
    members->setToolTip("Checking a class moves it from its previous combat-affinity group.");
    auto leaderPanel=new QVBoxLayout;body->addLayout(leaderPanel,2);leaderPanel->addWidget(new QLabel("Leader classes and strengths"));
    auto leaderSearch=new QLineEdit;leaderSearch->setPlaceholderText("Find a leader class…");leaderPanel->addWidget(leaderSearch);
    auto leaders=new QTableWidget;leaders->setObjectName("combatAffinityLeaders");leaders->setColumnCount(2);
    leaders->setHorizontalHeaderLabels({"Current class","Strength"});leaders->verticalHeader()->hide();
    leaders->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    leaders->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    leaders->setEditTriggers(QAbstractItemView::NoEditTriggers);leaderPanel->addWidget(leaders,1);
    auto notice=wrapped();notice->setObjectName("combatAffinityError");root->addWidget(notice);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);root->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const auto &classes=project->records(Table::Classes);QVector<int> ids;
    for(int id=1;id<classes.size();++id)if(!project->contentDeleted("classes",id))ids.append(id);
    auto label=[&](int id){return QString("%1 · %2").arg(id,2,16,QChar('0')).arg(classes[id].name.simplified());};
    bool loading=false;
    auto filter=[&] {
        for(int row=0;row<members->count();++row)members->item(row)->setHidden(!members->item(row)->text().contains(memberSearch->text(),Qt::CaseInsensitive));
        for(int row=0;row<leaders->rowCount();++row)leaders->setRowHidden(row,!leaders->item(row,0)->text().contains(leaderSearch->text(),Qt::CaseInsensitive));
    };
    std::function<void()> refresh=[&] {
        loading=true;const int group=groups->currentRow();const bool valid=group>=0 && group<next.groups().size();
        members->clear();leaders->setRowCount(0);members->setEnabled(valid);leaders->setEnabled(valid);
        rename->setEnabled(valid);remove->setEnabled(valid);add->setEnabled(next.groups().size()<255);
        if(valid)for(int id:ids) {
            const int current=next.groupFor(id);auto text=label(id);
            if(current>=0 && current!=group)text+=" — "+next.groups()[current].name;
            auto item=new QListWidgetItem(text,members);item->setData(Qt::UserRole,id);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
            item->setCheckState(current==group?Qt::Checked:Qt::Unchecked);
            const int row=leaders->rowCount();leaders->insertRow(row);auto lead=new QTableWidgetItem(label(id));lead->setData(Qt::UserRole,id);
            lead->setFlags((lead->flags()&~Qt::ItemIsEditable)|Qt::ItemIsUserCheckable);
            lead->setCheckState(next.strength(id,group)?Qt::Checked:Qt::Unchecked);leaders->setItem(row,0,lead);
            auto spin=new QSpinBox;spin->setObjectName(QString("combatAffinityLeaderStrength%1").arg(id));spin->setRange(-100,100);
            spin->setKeyboardTracking(false);spin->setValue(next.strength(id,group));spin->setEnabled(lead->checkState()==Qt::Checked);
            spin->setToolTip("0 removes this leader's bonus for the group.");leaders->setCellWidget(row,1,spin);
            QObject::connect(spin,&QSpinBox::valueChanged,&dialog,[&,id,group,row](int strength) {
                if(loading)return;
                try {next.set(id,group,strength);loading=true;leaders->item(row,0)->setCheckState(strength?Qt::Checked:Qt::Unchecked);
                    static_cast<QSpinBox *>(leaders->cellWidget(row,1))->setEnabled(strength!=0);loading=false;notice->clear();}
                catch(const Error &e){loading=false;notice->setText(QString::fromUtf8(e.what()));}
            });
        }
        loading=false;filter();
    };
    auto reloadGroups=[&](int selected) {
        const QSignalBlocker blocked(groups);groups->clear();for(const auto &group:next.groups())groups->addItem(group.name);
        groups->setCurrentRow(qMin(selected,int(next.groups().size())-1));refresh();
    };
    QObject::connect(groups,&QListWidget::currentRowChanged,&dialog,[&]{refresh();});
    QObject::connect(memberSearch,&QLineEdit::textChanged,&dialog,[&]{filter();});
    QObject::connect(leaderSearch,&QLineEdit::textChanged,&dialog,[&]{filter();});
    QObject::connect(members,&QListWidget::itemChanged,&dialog,[&](QListWidgetItem *item) {
        if(loading)return;const int group=groups->currentRow(),id=item->data(Qt::UserRole).toInt();
        auto selected=next.groups()[group].members;
        if(item->checkState()==Qt::Checked) {if(!selected.contains(id))selected.append(id);}else selected.removeAll(id);
        try {next.editGroup(group,next.groups()[group].name,selected,int(classes.size()));
            const QSignalBlocker blocked(members);item->setText(label(id));notice->clear();}
        catch(const Error &e){notice->setText(QString::fromUtf8(e.what()));refresh();}
    });
    QObject::connect(leaders,&QTableWidget::itemChanged,&dialog,[&](QTableWidgetItem *item) {
        if(loading || item->column()!=0)return;
        const int group=groups->currentRow(),id=item->data(Qt::UserRole).toInt();
        try {const int value=item->checkState()==Qt::Checked?10:0;next.set(id,group,value);
            auto spin=static_cast<QSpinBox *>(leaders->cellWidget(item->row(),1));const QSignalBlocker blocked(spin);spin->setValue(value);spin->setEnabled(value!=0);notice->clear();}
        catch(const Error &e){notice->setText(QString::fromUtf8(e.what()));refresh();}
    });
    QObject::connect(add,&QPushButton::clicked,&dialog,[&] {
        bool ok=false;const auto name=QInputDialog::getText(&dialog,"Add combat-affinity group","Group name",QLineEdit::Normal,{},&ok).trimmed();if(!ok)return;
        try {next.addGroup(name,{},int(classes.size()));reloadGroups(int(next.groups().size())-1);notice->clear();}
        catch(const Error &e){notice->setText(QString::fromUtf8(e.what()));}
    });
    QObject::connect(rename,&QPushButton::clicked,&dialog,[&] {
        const int group=groups->currentRow();if(group<0)return;bool ok=false;
        const auto name=QInputDialog::getText(&dialog,"Rename combat-affinity group","Group name",QLineEdit::Normal,next.groups()[group].name,&ok).trimmed();if(!ok)return;
        try {next.editGroup(group,name,next.groups()[group].members,int(classes.size()));reloadGroups(group);notice->clear();}
        catch(const Error &e){notice->setText(QString::fromUtf8(e.what()));}
    });
    QObject::connect(remove,&QPushButton::clicked,&dialog,[&] {
        const int group=groups->currentRow();if(group<0)return;next.removeGroup(group);reloadGroups(group);notice->clear();
    });
    reloadGroups(0);
    return dialog.exec()==QDialog::Accepted?std::optional<LeaderAffinity>(next):std::nullopt;
}
}
LeaderAffinityEditor::LeaderAffinityEditor(Project *project, QUndoStack *undo,
                                           std::function<void()> changed)
    : project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("leaderAffinityEditor");project_->loadTools();
    auto outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);
    auto group=new QGroupBox("Leader affinity — combat bonuses");outer->addWidget(group);auto body=new QVBoxLayout(group);
    scope_=wrapped();scope_->setObjectName("leaderAffinityScope");body->addWidget(scope_);
    body->addWidget(wrapped("Affects squad members' attack power, magic power, defense, accuracy and evasion. "
        "+15 raises a neutral 100% multiplier to 115%."));
    table_=new QTableWidget(0,3);table_->setObjectName("leaderAffinityTable");
    table_->setHorizontalHeaderLabels({"Group","Member classes","Strength"});table_->verticalHeader()->hide();
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setWordWrap(true);table_->setMinimumHeight(230);
    body->addWidget(table_);
    auto actions=new QHBoxLayout;body->addLayout(actions);
    manage_=new QPushButton("Manage groups…");manage_->setObjectName("leaderAffinityManageGroups");actions->addWidget(manage_);
    retail_=new QPushButton("Restore retail strengths");retail_->setObjectName("leaderAffinityRetail");actions->addWidget(retail_);
    retail_->setToolTip("Restores strengths for groups whose membership still matches an original group.");
    notice_=wrapped();notice_->setObjectName("leaderAffinityNotice");body->addWidget(notice_);
    connect(manage_,&QPushButton::clicked,this,[this] {
        if(const auto edited=manageGroups(project_,project_->tools()->leaderAffinity(),this))write(*edited);
    });
    connect(retail_,&QPushButton::clicked,this,[this] {
        if(leader_<1)return;auto next=project_->tools()->leaderAffinity();const auto values=LeaderAffinity::retail(leader_);
        for(int group=0;group<next.groups().size();++group)for(int original=0;original<4;++original)
            if(next.groups()[group].members==LeaderAffinity::members(original))next.set(leader_,group,values[original]);
        write(next);
    });
    connect(undo_,&QUndoStack::indexChanged,this,[this]{refresh();});refresh();
}
void LeaderAffinityEditor::selectClass(int id) {selected_=id;refresh();}
void LeaderAffinityEditor::refresh() {
    const auto &records=project_->records(Table::Classes);const bool valid=selected_>0 && selected_<records.size();
    leader_=valid?selected_:-1;
    if(valid) {const auto schema=fields(Table::Classes);for(int i=0;i<schema.size();++i)if(schema[i].key=="classCopyMatch") {
        const int link=project_->value(Table::Classes,selected_,i);if(link)leader_=link;break;
    }}
    const auto &affinity=project_->tools()->leaderAffinity();const bool available=valid && leader_>0 && leader_<records.size() && affinity.unavailable().isEmpty();
    auto name=[&](int id){return id>0 && id<records.size()?records[id].name.simplified():QString("Class %1").arg(id);};
    scope_->setText(!valid?"Select a class to see its leader bonuses.":leader_!=selected_
        ?QString("This class starts with %1 as its current class. These bonuses are shared by leaders using %1.").arg(name(leader_))
        :QString("Bonuses for %1 as the leader's current class.").arg(name(leader_)));
    notice_->setText(affinity.unavailable());manage_->setEnabled(affinity.unavailable().isEmpty());
    if(table_->rowCount()!=affinity.groups().size()) {
        table_->setRowCount(0);strength_.clear();effects_.clear();table_->setRowCount(int(affinity.groups().size()));
        for(int row=0;row<table_->rowCount();++row) {
            table_->setItem(row,0,new QTableWidgetItem);table_->setItem(row,1,new QTableWidgetItem);
            auto cell=new QWidget;auto layout=new QVBoxLayout(cell);layout->setContentsMargins(4,4,4,4);
            auto strength=new QSpinBox;strength->setObjectName(QString("leaderAffinityStrength%1").arg(row));strength->setRange(-100,100);strength->setKeyboardTracking(false);
            layout->addWidget(strength);auto effect=wrapped();layout->addWidget(effect);table_->setCellWidget(row,2,cell);strength_.append(strength);effects_.append(effect);
            connect(strength,&QSpinBox::valueChanged,this,[this,row](int value) {
                if(leader_<1)return;
                try {auto next=project_->tools()->leaderAffinity();next.set(leader_,row,value);write(next);}
                catch(const Error &e){refresh();notice_->setText(QString::fromUtf8(e.what()));}
            });
        }
    }
    bool canRestore=false;const auto originalValues=LeaderAffinity::retail(leader_);
    for(int row=0;row<affinity.groups().size();++row) {
        table_->item(row,0)->setText(affinity.groups()[row].name);QStringList names;
        for(int id:affinity.groups()[row].members)names<<name(id);
        table_->item(row,1)->setText(names.isEmpty()?"No members":names.join(", "));
        const QSignalBlocker blocked(strength_[row]);const auto value=affinity.strength(leader_,row);
        strength_[row]->setEnabled(available);strength_[row]->setValue(value);
        effects_[row]->setText(available?QString("100% → %1%").arg(100+value):QString());
        for(int original=0;original<4;++original)if(affinity.groups()[row].members==LeaderAffinity::members(original) && value!=originalValues[original])canRestore=true;
    }
    table_->resizeRowsToContents();retail_->setEnabled(available && canRestore);
}
void LeaderAffinityEditor::write(const LeaderAffinity &affinity) {
    auto next=project_->tools()->json();next["leaderAffinity"]=affinity.json();
    if(next!=project_->tools()->json()) {undo_->push(new AffinityCommand(project_,next));changed_();}
    refresh();
}
}
