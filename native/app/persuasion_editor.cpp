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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
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
    auto label = new QLabel(value);
    label->setWordWrap(true); label->setTextFormat(Qt::PlainText);
    return label;
}
class ScoreDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override {
        auto spin = new QSpinBox(parent); spin->setRange(0, 100); spin->setKeyboardTracking(false);
        connect(spin, &QSpinBox::valueChanged, spin, [spin] { spin->setProperty("scoreEdited", true); });
        connect(spin->findChild<QLineEdit *>(), &QLineEdit::textEdited, spin, [spin] { spin->setProperty("scoreEdited", true); });
        return spin;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        auto spin = static_cast<QSpinBox *>(editor);
        spin->setValue(index.data(Qt::EditRole).toInt());
        if (index.data(Qt::EditRole).toString() == "Mixed") spin->clear();
        spin->setProperty("scoreEdited", false);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        auto spin = static_cast<QSpinBox *>(editor);
        if (index.data(Qt::EditRole).toString() == "Mixed" && !spin->property("scoreEdited").toBool()) return;
        if (!spin->text().trimmed().isEmpty()) model->setData(index, spin->value(), Qt::EditRole);
    }
};
QString className(Project *p, int id) {
    const auto &rows = p->records(Table::Classes);
    return id > 0 && id < rows.size() ? rows[id].name.simplified() : QString("Class %1").arg(id);
}
QString groupName(Project *p, const PersuasionGroup &group) {
    if (group.members.size() == 1 && group.name == QString("Class %1").arg(group.members[0], 2, 16, QChar('0')))
        return className(p, group.members[0]);
    return group.name;
}
QString wrapped(const QString &name) {
    QStringList lines; QString line;
    for (const auto &word : name.split(' ', Qt::SkipEmptyParts)) {
        if (!line.isEmpty() && line.size() + word.size() > 19) { lines << line; line.clear(); }
        if (!line.isEmpty()) line += ' ';
        line += word;
    }
    if (!line.isEmpty()) lines << line;
    return lines.join('\n');
}
QString membersText(Project *project, const QVector<int> &members) {
    QStringList names;
    for (int id : members) names << className(project, id) + QString(" [%1]").arg(id, 2, 16, QChar('0'));
    return names.isEmpty() ? "No classes assigned" : names.join(", ");
}
}
PersuasionEditor::PersuasionEditor(Project *project, QUndoStack *undo,
    std::function<void(const Encounters &, const QString &)> onCommit)
    : project_(project), commit_(std::move(onCommit)) {
    setObjectName("persuasionEditor"); auto layout = new QVBoxLayout(this);
    layout->addWidget(text("Leader groups run across the top; target groups run down the left. "
        "Edit any cell to set the starting Talk affinity (0–100) for every leader in that group against that target group. "
        "Mixed means the leaders currently have different scores; entering a value gives them the same score."));
    auto actions = new QHBoxLayout;
    addLeader_ = new QPushButton("Add leader group..."); addLeader_->setObjectName("persuasionAddLeader");
    editLeader_ = new QPushButton("Edit leader group..."); editLeader_->setObjectName("persuasionEditLeader");
    addTarget_ = new QPushButton("Add target group..."); addTarget_->setObjectName("persuasionCreateGroup");
    editTarget_ = new QPushButton("Edit target group..."); editTarget_->setObjectName("persuasionEditGroup");
    actions->addWidget(addLeader_); actions->addWidget(editLeader_); actions->addSpacing(20);
    actions->addWidget(addTarget_); actions->addWidget(editTarget_); actions->addStretch(); layout->addLayout(actions);
    layout->addWidget(text("Targets ↓     Leaders →     Select a cell to see both groups' classes below."));
    table_ = new QTableWidget; table_->setObjectName("persuasionTable");
    table_->setItemDelegate(new ScoreDelegate(table_));
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setDefaultSectionSize(155);
    table_->horizontalHeader()->setMinimumSectionSize(120);
    table_->horizontalHeader()->setFixedHeight(86);
    table_->verticalHeader()->setDefaultSectionSize(38);
    table_->verticalHeader()->setMinimumSectionSize(32);
    table_->verticalHeader()->setMinimumWidth(235);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(table_, 1);
    members_ = new QPlainTextEdit; members_->setObjectName("persuasionMembers");
    members_->setReadOnly(true); members_->setFixedHeight(108); layout->addWidget(members_);
    layout->addWidget(text("For example, Beast Masters start at 80 for beasts; Beast Tamers start at 60. "
        "Target health then changes the score. This table applies across all maps and uses current class IDs. "
        "Custom squads with a fixed Talk chance use their own settings. The Ogre's special recruitment is unchanged."));
    notice_ = text({}); notice_->setObjectName("persuasionNotice"); layout->addWidget(notice_);
    connect(addLeader_, &QPushButton::clicked, this, [this] { editGroup(true, true); });
    connect(editLeader_, &QPushButton::clicked, this, [this] { editGroup(true, false); });
    connect(addTarget_, &QPushButton::clicked, this, [this] { editGroup(false, true); });
    connect(editTarget_, &QPushButton::clicked, this, [this] { editGroup(false, false); });
    connect(table_, &QTableWidget::currentCellChanged, this, [this] { showSelection(); });
    connect(table_->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int column) { table_->setCurrentCell(std::max(0, table_->currentRow()), column); });
    connect(table_->verticalHeader(), &QHeaderView::sectionClicked, this, [this](int row) { table_->setCurrentCell(row, std::max(0, table_->currentColumn())); });
    connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        bool ok = false; const int value = item->data(Qt::EditRole).toInt(&ok);
        const int column = item->column(), row = item->row();
        if (!ok) { refresh(); return; }
        const int leader = table_->horizontalHeaderItem(column)->data(Qt::UserRole).toInt();
        try { auto next = project_->encounters()->persuasion(); next.setGroupScore(leader, row, value); commit(next, "Change persuasion affinity"); }
        catch (const Error &e) { refresh(); notice_->setText(QString::fromUtf8(e.what())); notice_->show(); }
    });
    watchContentChanges(undo, this, [this] { refresh(); }); refresh();
}
int PersuasionEditor::selectedLeader() const {
    const auto item = table_->horizontalHeaderItem(table_->currentColumn());
    return item ? item->data(Qt::UserRole).toInt() : -1;
}
void PersuasionEditor::refresh() {
    const auto &affinity = project_->encounters()->persuasion(); const auto leaders = affinity.leaderGroups();
    const int row = table_->currentRow(), column = table_->currentColumn();
    const int scroll = table_->horizontalScrollBar()->value();
    const int verticalScroll = table_->verticalScrollBar()->value();
    const bool enabled = affinity.unavailable().isEmpty() && !affinity.groups().isEmpty();
    {
        const QSignalBlocker block(table_);
        table_->setRowCount(affinity.groups().size()); table_->setColumnCount(enabled ? leaders.size() + 1 : 0);
        for (int target = 0; target < affinity.groups().size(); ++target) {
            auto label = new QTableWidgetItem(affinity.groups()[target].name);
            label->setToolTip(affinity.groups()[target].name); table_->setVerticalHeaderItem(target, label);
        }
        for (int col = 0; col < table_->columnCount(); ++col) {
            const int group = col < leaders.size() ? col : -1;
            const QString name = group < 0 ? "Other leaders" : groupName(project_, leaders[group]);
            auto label = new QTableWidgetItem(wrapped(name)); label->setData(Qt::UserRole, group);
            label->setToolTip(group < 0 ? "Classes without an explicit leader entry" : membersText(project_, leaders[group].members));
            table_->setHorizontalHeaderItem(col, label);
            for (int target = 0; target < affinity.groups().size(); ++target) {
                const auto ids = group < 0 ? QVector<int>{-1} : leaders[group].members;
                const int value = affinity.score(ids.first(), target);
                bool mixed = false; QStringList details;
                for (int id : ids) {
                    const int current = affinity.score(id, target); mixed |= current != value;
                    details << (id < 0 ? "Other leaders" : className(project_, id)) + ": " + QString::number(current);
                }
                auto item = new QTableWidgetItem;
                item->setData(Qt::EditRole, mixed ? QVariant("Mixed") : QVariant(value));
                item->setTextAlignment(Qt::AlignCenter); item->setToolTip(details.join('\n'));
                table_->setItem(target, col, item);
            }
        }
        table_->setEnabled(enabled);
        if (enabled) table_->setCurrentCell(qBound(0, row < 0 ? affinity.groupFor(0x45) : row, table_->rowCount()-1), qBound(0, column, table_->columnCount()-1));
    }
    table_->horizontalScrollBar()->setValue(scroll);
    table_->verticalScrollBar()->setValue(verticalScroll);
    addLeader_->setEnabled(enabled); addTarget_->setEnabled(enabled && affinity.groups().size() < 32);
    notice_->setText(affinity.unavailable()); notice_->setVisible(!affinity.unavailable().isEmpty()); showSelection();
}
void PersuasionEditor::showSelection() {
    const auto &affinity = project_->encounters()->persuasion(); const auto leaders = affinity.leaderGroups();
    const int leader = selectedLeader(), target = table_->currentRow();
    const bool enabled = affinity.unavailable().isEmpty() && target >= 0;
    editLeader_->setEnabled(enabled && leader >= 0); editTarget_->setEnabled(enabled && target > 0);
    if (!enabled) { members_->clear(); return; }
    QVector<int> targets, fallback;
    const auto explicitLeaders = affinity.leaders();
    for (int id = 1; id < project_->records(Table::Classes).size() && id <= 255; ++id) {
        if (affinity.groupFor(id) == target) targets << id;
        if (!explicitLeaders.contains(id)) fallback << id;
    }
    members_->setPlainText("Leaders — " + (leader < 0 ? QString("Other leaders") : groupName(project_, leaders[leader])) + ": " +
        membersText(project_, leader < 0 ? fallback : leaders[leader].members) + "\n\nTargets — " + affinity.groups()[target].name + ": " + membersText(project_, targets));
}
void PersuasionEditor::commit(const Persuasion &value, const QString &label) {
    auto next = *project_->encounters(); next.setPersuasion(value.json(), project_->records(Table::Classes).size());
    if (next.json() != project_->encounters()->json()) commit_(next, label);
    refresh();
}
void PersuasionEditor::editGroup(bool leader, bool create) {
    const auto current = project_->encounters()->persuasion();
    const auto groups = leader ? current.leaderGroups() : current.groups();
    const int selected = leader ? selectedLeader() : table_->currentRow();
    if (!create && (selected < 0 || (!leader && selected == 0))) return;
    QDialog dialog(this); dialog.setObjectName("persuasionGroupDialog");
    dialog.setWindowTitle(QString(create ? "Add %1 group" : "Edit %1 group").arg(leader ? "leader" : "target"));
    dialog.resize(560, 650); auto layout = new QVBoxLayout(&dialog); auto form = new QFormLayout;
    auto name = new QLineEdit(create ? QString() : groups[selected].name);
    name->setObjectName("persuasionGroupName"); name->setMaxLength(80); form->addRow("Group name", name);
    auto copy = new QComboBox(&dialog); copy->setObjectName("persuasionCopyGroup");
    for (const auto &g : current.groups()) copy->addItem(g.name);
    copy->setCurrentIndex(std::max(0, table_->currentRow()));
    if (create && !leader) form->addRow("Copy starting scores from", copy); else copy->hide();
    layout->addLayout(form);
    layout->addWidget(text(leader ?
        "Select the classes in this leader group. Existing scores are kept; new leaders start with Other leaders scores. "
        "Selected classes move out of their previous leader group. Removed classes keep their scores in individual columns. "
        "This does not change target groups or grant the ability to lead." :
        "Select the target classes in this group. Selected classes move out of their previous target group; "
        "removed classes return to Other classes. Leader groups and class families are unchanged."));
    auto search = new QLineEdit; search->setPlaceholderText("Find classes by name or hex ID..."); layout->addWidget(search);
    auto list = new QListWidget; list->setObjectName("persuasionGroupMembers"); layout->addWidget(list, 1);
    for (int id = 1; id < project_->records(Table::Classes).size() && id <= 255; ++id) {
        auto item = new QListWidgetItem(className(project_, id) + QString(" [%1]").arg(id, 2, 16, QChar('0')), list);
        item->setData(Qt::UserRole, id); item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(!create && groups[selected].members.contains(id) ? Qt::Checked : Qt::Unchecked);
    }
    connect(search, &QLineEdit::textChanged, &dialog, [list](const QString &query) {
        for (int row = 0; row < list->count(); ++row) list->item(row)->setHidden(!list->item(row)->text().contains(query.trimmed(), Qt::CaseInsensitive));
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel); layout->addWidget(buttons);
    auto validate = [=] {
        bool hasMember = false;
        for (int row = 0; row < list->count(); ++row) hasMember |= list->item(row)->checkState() == Qt::Checked;
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!name->text().trimmed().isEmpty() && (!leader || hasMember));
    };
    connect(name, &QLineEdit::textChanged, &dialog, validate); connect(list, &QListWidget::itemChanged, &dialog, validate); validate();
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    QVector<int> members;
    for (int row = 0; row < list->count(); ++row)
        if (list->item(row)->checkState() == Qt::Checked) members << list->item(row)->data(Qt::UserRole).toInt();
    try {
        auto next = current; const int count = project_->records(Table::Classes).size();
        if (leader) next.editLeaderGroup(create ? -1 : selected, name->text(), members, count);
        else if (create) next.addGroup(name->text(), members, copy->currentIndex(), count);
        else next.editGroup(selected, name->text(), members, count);
        commit(next, QString(create ? "Add persuasion %1 group" : "Edit persuasion %1 group").arg(leader ? "leader" : "target"));
        if (leader) {
            const auto updated = next.leaderGroups();
            for (int col = 0; col < updated.size(); ++col) if (updated[col].members.contains(members.first())) {
                table_->setCurrentCell(std::max(0, table_->currentRow()), col); break;
            }
        } else table_->setCurrentCell(create ? next.groups().size()-1 : selected, std::max(0, table_->currentColumn()));
    } catch (const Error &e) { notice_->setText(QString::fromUtf8(e.what())); notice_->show(); }
}
}
