#include "app/save_unit_editor.h"
#include "app/save_editor.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <algorithm>

namespace lordly {
SaveUnitEditor::SaveUnitEditor(SaveEditor *editor) : QWidget(editor), editor_(editor) {
    setObjectName("saveSquads");
    auto layout = new QVBoxLayout(this);
    note_ = new QLabel;
    note_->setWordWrap(true);
    layout->addWidget(note_);
    auto split = new QSplitter;
    layout->addWidget(split, 1);
    units_ = new QListWidget;
    units_->setObjectName("saveUnitList");
    split->addWidget(units_);
    auto detail = new QWidget;
    auto body = new QVBoxLayout(detail);
    members_ = new QListWidget;
    members_->setObjectName("saveUnitMembers");
    body->addWidget(members_);
    auto help =
        new QLabel("Select a member, then choose a formation cell. Occupied cells swap members.");
    help->setWordWrap(true);
    body->addWidget(help);
    auto grid = new QGridLayout;
    for (int display = 0; display < 9; ++display) {
        const int cell = 8 - display;
        auto button = new QPushButton;
        button->setObjectName(QString("saveUnitCell%1").arg(cell));
        button->setMinimumSize(100, 68);
        grid->addWidget(button, display / 3, display % 3);
        grid_.prepend(button);
        connect(button, &QPushButton::clicked, this, [this, cell] {
            const int member = members_->currentRow();
            if (loading_ || member < 0 || member >= ids_.size() || !ids_[member])
                return;
            const int from = cells_[member];
            for (int i = 0; i < ids_.size(); ++i)
                if (i != member && ids_[i] && cells_[i] == cell)
                    cells_[i] = from;
            cells_[member] = cell;
            drawFormation();
        });
    }
    body->addLayout(grid);
    auto form = new QFormLayout;
    unity_ = new QSpinBox;
    unity_->setObjectName("saveUnitUnity");
    unity_->setRange(0, 100);
    tactic_ = new QComboBox;
    tactic_->setObjectName("saveUnitTactic");
    form->addRow("Unity", unity_);
    form->addRow("Tactic", tactic_);
    body->addLayout(form);
    apply_ = new QPushButton("Apply squad changes");
    apply_->setObjectName("saveUnitApply");
    body->addWidget(apply_);
    split->addWidget(detail);
    split->setStretchFactor(1, 2);
    connect(units_, &QListWidget::currentRowChanged, this, [this] { showUnit(); });
    connect(members_, &QListWidget::currentRowChanged, this, [this] { drawFormation(); });
    connect(apply_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->editUnit(units_->currentItem()->data(Qt::UserRole).toInt(), cells_,
                              unity_->value(), tactic_->currentData().toInt());
    });
    connect(editor_, &SaveEditor::refreshed, this, [this] { refresh(); });
    refresh();
}
QString SaveUnitEditor::memberName(int id) const {
    if (!id)
        return "Empty";
    if (id >= 220)
        return QString("Unknown member %1").arg(id);
    if (id >= 100)
        return QString("Soldiers %1").arg(id - 99);
    try {
        return editor_->document()->value(SaveTable::Characters, id, "name").toString();
    } catch (const Error &) {
        return QString("Missing character %1").arg(id);
    }
}
void SaveUnitEditor::refresh() {
    loading_ = true;
    const int prior =
        units_->currentItem() ? units_->currentItem()->data(Qt::UserRole).toInt() : -1;
    units_->clear();
    if (editor_->document())
        for (const auto &unit : editor_->document()->units()) {
            auto item = new QListWidgetItem(
                QString("Squad %1 · %2").arg(unit.index + 1).arg(memberName(unit.members[0])),
                units_);
            item->setData(Qt::UserRole, unit.index);
            if (unit.index == prior)
                units_->setCurrentItem(item);
        }
    if (!units_->currentItem() && units_->count())
        units_->setCurrentRow(0);
    loading_ = false;
    showUnit();
}
void SaveUnitEditor::showUnit() {
    if (loading_)
        return;
    loading_ = true;
    members_->clear();
    ids_.clear();
    cells_.clear();
    tactic_->clear();
    for (const auto &label : {"Autonomous", "Attack Strongest", "Attack Leader", "Attack Weakest"})
        tactic_->addItem(label, tactic_->count());
    if (editor_->document() && units_->currentItem()) {
        const int index = units_->currentItem()->data(Qt::UserRole).toInt();
        for (const auto &unit : editor_->document()->units()) {
            if (unit.index != index)
                continue;
            ids_ = unit.members;
            cells_ = unit.cells;
            for (int i = 0; i < 5; ++i) {
                auto item = new QListWidgetItem(memberName(ids_[i]) + (i == 0 ? " · Leader" : ""),
                                                members_);
                if (!ids_[i])
                    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            }
            unity_->setRange(0, std::max(100, unit.unity));
            unity_->setValue(unit.unity);
            if (tactic_->findData(unit.tactic) < 0)
                tactic_->addItem(QString("Keep unknown (%1)").arg(unit.tactic), unit.tactic);
            tactic_->setCurrentIndex(tactic_->findData(unit.tactic));
            members_->setCurrentRow(0);
            break;
        }
    }
    const bool editable = editor_->document() && editor_->document()->canEditUnits();
    note_->setText(editable ? "Edit the saved squad's formation, Unity and tactic. Member "
                              "identities and the leader are retained."
                            : "Squads are read-only in suspend saves and runtime states. Formation "
                              "edits require a regular battery-save slot.");
    apply_->setEnabled(editable && !ids_.isEmpty());
    unity_->setEnabled(apply_->isEnabled());
    tactic_->setEnabled(apply_->isEnabled());
    loading_ = false;
    drawFormation();
}
void SaveUnitEditor::drawFormation() {
    if (loading_)
        return;
    for (int cell = 0; cell < 9; ++cell) {
        QStringList names;
        bool selected = false;
        for (int member = 0; member < ids_.size(); ++member)
            if (ids_[member] && cells_[member] == cell) {
                names << memberName(ids_[member]) + (member == 0 ? " · Leader" : "");
                selected |= members_->currentRow() == member;
            }
        grid_[cell]->setText(QString("%1\n%2").arg(cell + 1).arg(names.join(" + ")));
        grid_[cell]->setStyleSheet(selected ? "background:#315a82;color:white" : "");
        grid_[cell]->setEnabled(apply_->isEnabled());
    }
}
} // namespace lordly
