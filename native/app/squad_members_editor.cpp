#include "app/squad_members_editor.h"
#include "app/formation_board.h"
#include <QComboBox>
#include <QCompleter>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QSet>

namespace lordly {
static QComboBox *choice(const QString &name) {
    auto box = new QComboBox;
    box->setObjectName(name);
    box->setEditable(true);
    box->setInsertPolicy(QComboBox::NoInsert);
    box->completer()->setCompletionMode(QCompleter::PopupCompletion);
    box->completer()->setFilterMode(Qt::MatchContains);
    box->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    return box;
}
static int selected(QComboBox *box) {
    if (box->currentIndex() < 0 || box->currentText() != box->itemText(box->currentIndex()))
        throw Error("Choose a listed value or finish the search before applying");
    return box->currentData().toInt();
}
SquadMembersEditor::SquadMembersEditor(Project *project, const QString &prefix,
                                     int minimumMembers, int minimumClass, QWidget *parent)
    : QWidget(parent), project_(project), minimumClass_(minimumClass) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto countRow = new QHBoxLayout;
    count_ = new QSpinBox;
    count_->setObjectName(prefix + "MemberCount");
    count_->setRange(minimumMembers, 5);
    countRow->addWidget(new QLabel("Members"));
    countRow->addWidget(count_);
    countRow->addStretch();
    layout->addLayout(countRow);
    table_ = new QTableWidget(5, 4);
    table_->setObjectName(prefix + "Members");
    table_->setHorizontalHeaderLabels({"Class", "Level offset", "Equipment choice 1", "Equipment choice 2"});
    table_->setVerticalHeaderLabels({"Leader", "2", "3", "4", "5"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_);
    for (int i = 0; i < 5; ++i) {
        table_->setRowHeight(i, 36);
        auto cls = choice(prefix + QString("Class_%1").arg(i));
        classes_ << cls;
        table_->setCellWidget(i, 0, cls);
        auto level = new QSpinBox;
        level->setObjectName(prefix + QString("Level_%1").arg(i));
        level->setRange(-128, 127);
        levels_ << level;
        table_->setCellWidget(i, 1, level);
        positions_ << 8 - i * 2;
        for (int e = 0; e < 2; ++e) {
            auto item = choice(prefix + QString("Equipment_%1_%2").arg(i).arg(e));
            equipment_ << item;
            table_->setCellWidget(i, e + 2, item);
            connect(item, &QComboBox::currentIndexChanged, this, [this, i] {
                if (!loading_) { table_->setCurrentCell(i, 0); preview(); emit edited(); }
            });
            connect(item, &QComboBox::editTextChanged, this, [this] { if (!loading_) preview(); });
        }
        connect(cls, &QComboBox::currentIndexChanged, this, [this, i] {
            if (loading_) return;
            table_->setCurrentCell(i, 0);
            equipmentChoices(i, {equipment_[i*2]->currentData().toInt(), equipment_[i*2+1]->currentData().toInt()});
            preview(); emit edited();
        });
        connect(cls, &QComboBox::editTextChanged, this, [this] { if (!loading_) preview(); });
        connect(level, &QSpinBox::valueChanged, this, [this, i] {
            if (!loading_) { table_->setCurrentCell(i, 0); preview(); emit edited(); }
        });
    }
    auto hint = new QLabel("Drag a unit to move or swap members. You can also select a member and click an empty cell. "
                          "Large members use two slots and need adjacent cells clear. Equipment choices follow the game's "
                          "compatibility rules; the second choice takes precedence when both fit the same slot.");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    board_ = new FormationBoard(project, prefix + "Cell_");
    layout->addWidget(board_, 0, Qt::AlignLeft);
    for (int cell = 0; cell < 9; ++cell)
        connect(board_->cell(cell), &QPushButton::clicked, this, [this, cell] {
            moveMember(qBound(0, table_->currentRow(), count_->value()-1), cell);
        });
    connect(board_, &FormationBoard::cellPressed, this, [this](int cell) {
        for (int i = 0; i < count_->value(); ++i)
            if (positions_[i] == cell) { table_->setCurrentCell(i, 0); break; }
    });
    connect(board_, &FormationBoard::cellMoved, this, [this](int from, int to) {
        for (int i = 0; i < count_->value(); ++i)
            if (positions_[i] == from) { moveMember(i, to); break; }
    });
    notice_ = new QLabel;
    notice_->setObjectName(prefix + "FormationNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    connect(count_, &QSpinBox::valueChanged, this, [this] { changeCount(); });
    connect(table_, &QTableWidget::currentCellChanged, this, [this] { if (!loading_) preview(); });
}
QVector<CustomMember> SquadMembersEditor::members() const {
    QVector<CustomMember> result;
    for (int i = 0; i < count_->value(); ++i)
        result << CustomMember{selected(classes_[i]), levels_[i]->value(), positions_[i],
                               {selected(equipment_[i*2]), selected(equipment_[i*2+1])}};
    return result;
}
void SquadMembersEditor::setMembers(const QVector<CustomMember> &values) {
    loading_ = true;
    count_->setValue(int(values.size()));
    shownCount_ = count_->value();
    QStringList labels;
    for (int id = minimumClass_; id < project_->records(Table::Classes).size(); ++id)
        labels << QString("%1 · %2").arg(id).arg(QString(project_->record(Table::Classes,id).name).replace('\n',' '));
    for (int i = 0; i < 5; ++i) {
        const auto m = i < values.size() ? values[i] : CustomMember{5, 0, 8-i*2, {0, 0}};
        auto box = classes_[i];
        if (classLabels_ != labels) {
            box->clear();
            for (int row = 0; row < labels.size(); ++row) box->addItem(labels[row], minimumClass_ + row);
        }
        if (box->findData(m.classId) < 0) box->addItem(QString("Original class %1").arg(m.classId),m.classId);
        box->setCurrentIndex(box->findData(m.classId));
        levels_[i]->setValue(m.levelOffset);
        levels_[i]->setEnabled(true);
        positions_[i] = m.cell;
        equipmentChoices(i, m.equipment);
    }
    classLabels_ = labels;
    table_->setCurrentCell(0, 0);
    loading_ = false;
    preview();
}
void SquadMembersEditor::setLevelEditable(int member, bool enabled) { levels_.at(member)->setEnabled(enabled); }
void SquadMembersEditor::equipmentChoices(int member, const std::array<int,2> &values) {
    const int cls = classes_[member]->currentData().toInt();
    QSet<int> types;
    if (cls >= 0 && cls < project_->records(Table::Classes).size()) {
        const int copy = u8(project_->record(Table::Classes,cls).bytes,69);
        const int effective = copy ? copy : cls;
        if (effective < project_->records(Table::Classes).size()) {
            const auto row = project_->record(Table::Classes,effective).bytes;
            for (int slot = 0; slot < 4; ++slot) {
                const int id = u16(row,46+slot*2);
                if (id > 0 && id < project_->records(Table::Items).size()) types.insert(u8(project_->record(Table::Items,id).bytes,4));
            }
        }
    }
    for (int e = 0; e < 2; ++e) {
        auto box = equipment_[member*2+e];
        QSignalBlocker block(box);
        box->clear(); box->addItem("None — keep class default",0);
        for (const auto &item : project_->records(Table::Items))
            if (item.id && types.contains(u8(item.bytes,4))) box->addItem(QString("%1 · %2").arg(item.id).arg(item.name.simplified()),item.id);
        if (box->findData(values[e]) < 0) {
            box->addItem(QString("%1 · existing incompatible choice").arg(values[e]),values[e]);
            qobject_cast<QStandardItemModel *>(box->model())->item(box->count()-1)->setEnabled(false);
        }
        box->setCurrentIndex(box->findData(values[e]));
    }
}
void SquadMembersEditor::moveMember(int member, int cell) {
    if (loading_ || member < 0 || member >= count_->value()) return;
    const auto before = positions_;
    for (int i = 0; i < count_->value(); ++i)
        if (i != member && positions_[i] == cell) positions_[i] = before[member];
    positions_[member] = cell;
    try {
        validateSquadFormation(members(),project_->classData());
        table_->setCurrentCell(member,0); preview(); emit edited();
    } catch (const Error &e) {
        positions_ = before; preview(); notice_->setText(QString::fromUtf8(e.what()));
    }
}
void SquadMembersEditor::changeCount() {
    if (loading_) return;
    const auto before = positions_;
    try {
        for (int i = shownCount_; i < count_->value(); ++i) {
            bool placed = false;
            for (int cell = 8; cell >= 0; --cell) {
                positions_[i] = cell;
                auto value = members(); value.resize(i+1);
                try { validateSquadFormation(value,project_->classData()); placed = true; break; }
                catch (const Error &) {}
            }
            if (!placed) throw Error("No valid free formation cell. Move members or use fewer large classes first.");
        }
        shownCount_ = count_->value(); preview(); emit edited();
    } catch (const Error &e) {
        positions_ = before;
        QSignalBlocker block(count_); count_->setValue(shownCount_);
        preview(); notice_->setText(QString::fromUtf8(e.what()));
    }
}
void SquadMembersEditor::preview() {
    if (loading_) return;
    for (int i = 0; i < 5; ++i) table_->setRowHidden(i, i >= count_->value());
    table_->setFixedHeight(table_->horizontalHeader()->height()+count_->value()*36+2*table_->frameWidth());
    board_->clearUnits();
    try {
        const auto value = members();
        int cost = 0;
        for (int i = 0; i < value.size(); ++i) {
            const auto &m = value[i];
            const auto name = QString(project_->record(Table::Classes,m.classId).name).replace('\n',' ');
            board_->setUnit(m.cell,m.classId,QString("%1 · %2%3").arg(i+1).arg(name).arg(i ? "" : " (Leader)"),table_->currentRow()==i);
            const int size = u8(project_->record(Table::Classes,m.classId).bytes,4);
            cost += size == 2 ? 2 : 1;
            if (size == 2)
                for (int cell = 0; cell < 9; ++cell)
                    if (std::abs(cell/3-m.cell/3)<=1 && std::abs(cell%3-m.cell%3)<=1 && cell!=m.cell)
                        board_->setCellHint(cell,"Keep clear for the adjacent large member");
        }
        validateSquadFormation(value,project_->classData());
        notice_->setText(QString("%1 of 5 formation slots used. Front row is nearest you on the board.").arg(cost));
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
    board_->finishUpdate();
}
}
