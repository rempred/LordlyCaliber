#include "app/persuasion_editor.h"
#include "app/content_refresh.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QUndoStack>
#include <algorithm>

namespace lordly {
namespace {
QLabel *text(const QString &value) {
    auto label=new QLabel(value); label->setWordWrap(true); label->setTextFormat(Qt::PlainText); return label;
}
class ScoreDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent,const QStyleOptionViewItem &,const QModelIndex &) const override {
        auto spin=new QSpinBox(parent); spin->setRange(0,100); spin->setKeyboardTracking(false); return spin;
    }
    void setEditorData(QWidget *editor,const QModelIndex &index) const override {
        static_cast<QSpinBox *>(editor)->setValue(index.data(Qt::EditRole).toInt());
    }
    void setModelData(QWidget *editor,QAbstractItemModel *model,const QModelIndex &index) const override {
        model->setData(index,static_cast<QSpinBox *>(editor)->value(),Qt::EditRole);
    }
};
QString className(Project *p,int id) {
    const auto &rows=p->records(Table::Classes);
    return id>0 && id<rows.size() ? rows[id].name.simplified() : QString("Class %1").arg(id);
}
}
PersuasionEditor::PersuasionEditor(Project *project,QUndoStack *undo,
    std::function<void(const Encounters &,const QString &)> onCommit)
    : project_(project),commit_(std::move(onCommit)) {
    setObjectName("persuasionEditor"); auto layout=new QVBoxLayout(this);
    layout->addWidget(text("Choose a target group, then edit the starting Talk score for each leader. "
        "For example, Beast Master starts at 80 for beasts; Beast Tamer starts at 60. "
        "Target health then changes that score, so these numbers are not the final recruitment chance. "
        "Classes use their current class ID. Adding a row does not grant the ability to lead."));
    auto actions=new QHBoxLayout;
    actions->addWidget(new QLabel("Target group")); group_=new QComboBox; group_->setObjectName("persuasionGroup");
    actions->addWidget(group_,1);
    edit_=new QPushButton("Edit group..."); edit_->setObjectName("persuasionEditGroup"); actions->addWidget(edit_);
    create_=new QPushButton("Create group..."); create_->setObjectName("persuasionCreateGroup"); actions->addWidget(create_);
    layout->addLayout(actions);
    members_=text({}); members_->setObjectName("persuasionMembers"); layout->addWidget(members_);
    auto leaders=new QHBoxLayout;
    search_=new QLineEdit; search_->setPlaceholderText("Find a leader class..."); search_->setObjectName("persuasionSearch");
    leaders->addWidget(search_,1);
    add_=new QPushButton("Add leader row..."); add_->setObjectName("persuasionAddLeader"); leaders->addWidget(add_);
    layout->addLayout(leaders);
    table_=new QTableWidget(0,2); table_->setObjectName("persuasionTable");
    table_->setHorizontalHeaderLabels({"Leader class","Starting Talk score"}); table_->verticalHeader()->hide();
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table_->setItemDelegateForColumn(1,new ScoreDelegate(table_)); layout->addWidget(table_,1);
    layout->addWidget(text("These rules apply across all maps. A custom encounter squad set to a fixed Talk chance "
        "uses its own chance and leader bonuses instead. Use Original game calculation on that squad to use this table. "
        "The Ogre's special guaranteed recruitment is unchanged."));
    notice_=text({}); notice_->setObjectName("persuasionNotice"); layout->addWidget(notice_);
    connect(group_,&QComboBox::currentIndexChanged,this,[this] { showGroup(); });
    connect(search_,&QLineEdit::textChanged,this,[this] { showGroup(); });
    connect(add_,&QPushButton::clicked,this,[this] { addLeader(); });
    connect(create_,&QPushButton::clicked,this,[this] { editGroup(true); });
    connect(edit_,&QPushButton::clicked,this,[this] { editGroup(false); });
    connect(table_,&QTableWidget::itemChanged,this,[this](QTableWidgetItem *item) {
        if(item->column()!=1 || group_->currentIndex()<0) return;
        const int leader=table_->item(item->row(),0)->data(Qt::UserRole).toInt();
        try { auto next=project_->encounters()->persuasion(); next.setScore(leader,group_->currentIndex(),item->data(Qt::EditRole).toInt()); commit(next,"Change persuasion score"); }
        catch(const Error &e) { showGroup(); notice_->setText(QString::fromUtf8(e.what())); }
    });
    watchContentChanges(undo,this,[this] { refresh(); }); refresh();
}
void PersuasionEditor::refresh() {
    const auto &affinity=project_->encounters()->persuasion(); const int selected=group_->currentIndex();
    { const QSignalBlocker block(group_); group_->clear(); for(const auto &g:affinity.groups()) group_->addItem(g.name);
      if(group_->count()) group_->setCurrentIndex(qBound(0,selected < 0 ? affinity.groupFor(0x45) : selected,group_->count()-1)); }
    const bool enabled=affinity.unavailable().isEmpty() && !affinity.groups().isEmpty();
    group_->setEnabled(enabled); create_->setEnabled(enabled && affinity.groups().size()<32);
    add_->setEnabled(enabled && affinity.leaders().size()<std::min(255,int(project_->records(Table::Classes).size())-1));
    notice_->setText(affinity.unavailable()); showGroup();
}
void PersuasionEditor::showGroup() {
    const auto &affinity=project_->encounters()->persuasion(); const int group=group_->currentIndex();
    const QSignalBlocker block(table_); table_->setRowCount(0); edit_->setEnabled(group>0);
    if(group<0) { members_->clear(); return; }
    QStringList names;
    for(int id=1;id<project_->records(Table::Classes).size();++id) if(affinity.groupFor(id)==group) names<<className(project_,id);
    members_->setText(names.isEmpty() ? "No target classes assigned." : "Target classes: " + names.join(", "));
    auto rows=affinity.leaders(); rows.prepend(-1);
    const auto search=search_->text().trimmed();
    for(int leader:rows) {
        const QString name=leader==-1 ? "Other leaders — no individual row" : className(project_,leader);
        if(leader!=-1 && !name.contains(search,Qt::CaseInsensitive) && !QString::number(leader,16).contains(search,Qt::CaseInsensitive)) continue;
        const int row=table_->rowCount(); table_->insertRow(row);
        auto label=new QTableWidgetItem(name); label->setFlags(label->flags() & ~Qt::ItemIsEditable); label->setData(Qt::UserRole,leader);
        if(leader>0) label->setToolTip(QString("Current class ID %1 (hex)").arg(leader,2,16,QChar('0')));
        table_->setItem(row,0,label);
        auto score=new QTableWidgetItem; score->setData(Qt::EditRole,affinity.score(leader,group)); table_->setItem(row,1,score);
    }
}
void PersuasionEditor::commit(const Persuasion &value,const QString &label) {
    auto next=*project_->encounters(); next.setPersuasion(value.json(),project_->records(Table::Classes).size());
    if(next.json()!=project_->encounters()->json()) commit_(next,label);
    refresh();
}
void PersuasionEditor::addLeader() {
    QDialog dialog(this); dialog.setWindowTitle("Add persuasion leader"); auto layout=new QVBoxLayout(&dialog);
    layout->addWidget(text("Choose a current class. Its scores start from the Other leaders row in every group."));
    auto choices=new QComboBox; choices->setObjectName("persuasionNewLeader");
    const auto existing=project_->encounters()->persuasion().leaders();
    for(int id=1;id<project_->records(Table::Classes).size() && id<=255;++id)
        if(!existing.contains(id)) choices->addItem(className(project_,id)+QString(" [%1]").arg(id,2,16,QChar('0')),id);
    layout->addWidget(choices);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(choices->count()>0);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted) return;
    try { auto next=project_->encounters()->persuasion(); next.addLeader(choices->currentData().toInt()); commit(next,"Add persuasion leader"); }
    catch(const Error &e) { notice_->setText(QString::fromUtf8(e.what())); }
}
void PersuasionEditor::editGroup(bool create) {
    const auto current=project_->encounters()->persuasion(); const int selected=group_->currentIndex();
    if(selected<0 || (!create && selected==0)) return;
    QDialog dialog(this); dialog.setObjectName("persuasionGroupDialog"); dialog.setWindowTitle(create?"Create persuasion group":"Edit persuasion group");
    dialog.resize(520,620); auto layout=new QVBoxLayout(&dialog); auto form=new QFormLayout;
    auto name=new QLineEdit(create?QString():current.groups()[selected].name); name->setObjectName("persuasionGroupName"); name->setMaxLength(80); form->addRow("Group name",name);
    auto copy=new QComboBox; copy->setObjectName("persuasionCopyGroup");
    for(const auto &g:current.groups()) copy->addItem(g.name); copy->setCurrentIndex(selected);
    if(create) form->addRow("Copy starting scores from",copy); else copy->setParent(&dialog);
    copy->setVisible(create); layout->addLayout(form);
    layout->addWidget(text("Select target classes for this group. Selected classes move out of their previous group. "
        "Classes removed from this group return to Other classes. This does not change class families or combat affinity."));
    auto list=new QListWidget; list->setObjectName("persuasionGroupMembers"); layout->addWidget(list,1);
    for(int id=1;id<project_->records(Table::Classes).size() && id<=255;++id) {
        auto item=new QListWidgetItem(className(project_,id)+QString(" [%1]").arg(id,2,16,QChar('0')),list);
        item->setData(Qt::UserRole,id); item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
        item->setCheckState(!create && current.groupFor(id)==selected?Qt::Checked:Qt::Unchecked);
    }
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel); layout->addWidget(buttons);
    auto validate=[=] { buttons->button(QDialogButtonBox::Ok)->setEnabled(!name->text().trimmed().isEmpty()); };
    connect(name,&QLineEdit::textChanged,&dialog,validate); validate();
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted) return;
    QVector<int> members;
    for(int row=0;row<list->count();++row) if(list->item(row)->checkState()==Qt::Checked) members<<list->item(row)->data(Qt::UserRole).toInt();
    try {
        auto next=current; const int count=project_->records(Table::Classes).size();
        if(create) next.addGroup(name->text(),members,copy->currentIndex(),count); else next.editGroup(selected,name->text(),members,count);
        commit(next,create?"Create persuasion group":"Edit persuasion group");
        group_->setCurrentIndex(create?int(next.groups().size())-1:selected);
    } catch(const Error &e) { notice_->setText(QString::fromUtf8(e.what())); }
}
}
