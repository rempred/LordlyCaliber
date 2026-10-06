#include "app/save_unit_editor.h"
#include "app/save_editor.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
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
    members_->setMaximumHeight(150);
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
    auto draftActions = new QHBoxLayout;
    draftActions->addWidget(apply_);
    discard_ = new QPushButton("Discard squad changes");
    discard_->setObjectName("saveUnitDiscard");
    draftActions->addWidget(discard_);
    body->addLayout(draftActions);
    draftNote_ = new QLabel("Unapplied changes. Apply or discard them before changing squads, "
                            "save slots, other save fields, exporting, or using Undo.");
    draftNote_->setObjectName("saveUnitDraftNote");
    draftNote_->setWordWrap(true);
    body->addWidget(draftNote_);
    connect(discard_, &QPushButton::clicked, this, [this] { showUnit(); });
    auto transfers = new QFormLayout;
    targetUnit_ = new QComboBox;
    targetUnit_->setObjectName("saveSwapUnit");
    targetMember_ = new QComboBox;
    targetMember_->setObjectName("saveSwapMember");
    transfers->addRow("Destination squad", targetUnit_);
    transfers->addRow("Member or empty cell", targetMember_);
    body->addLayout(transfers);
    swap_ = new QPushButton("Swap selected members");
    swap_->setObjectName("saveSwapApply");
    body->addWidget(swap_);
    toReserve_ = new QPushButton("Move selected member to reserve");
    toReserve_->setObjectName("saveMoveToReserve");
    body->addWidget(toReserve_);
    auto reserve = new QFormLayout;
    reserveMember_ = new QComboBox;
    reserveMember_->setObjectName("saveReserveMember");
    reserveCell_ = new QComboBox;
    reserveCell_->setObjectName("saveReserveCell");
    reserve->addRow("Reserve character", reserveMember_);
    reserve->addRow("Empty cell in this squad", reserveCell_);
    body->addLayout(reserve);
    fromReserve_ = new QPushButton("Add reserve character to this squad");
    fromReserve_->setObjectName("saveAddFromReserve");
    body->addWidget(fromReserve_);
    auto suppliesGroup = new QGroupBox("Carried items");
    auto suppliesLayout = new QGridLayout(suppliesGroup);
    suppliesLayout->setColumnStretch(1, 1);
    suppliesLayout->setColumnStretch(3, 1);
    capacity_ = new QLabel;
    capacity_->setObjectName("saveUnitCapacity");
    capacity_->setWordWrap(true);
    suppliesLayout->addWidget(capacity_, 0, 0, 1, 4);
    for (int i = 0; i < 10; ++i) {
        auto choice = new QComboBox;
        choice->setObjectName(QString("saveUnitSupply%1").arg(i));
        choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        choice->setMinimumContentsLength(18);
        supplies_ << choice;
        suppliesLayout->addWidget(new QLabel(QString::number(i + 1)), i / 2 + 1, (i % 2) * 2);
        suppliesLayout->addWidget(choice, i / 2 + 1, (i % 2) * 2 + 1);
        connect(choice, &QComboBox::currentIndexChanged, this, [this] {
            if (loading_ || !units_->currentItem())
                return;
            QVector<int> values;
            for (const auto *box : supplies_)
                values << box->currentData().toInt();
            if (!editor_->editUnitSupplies(units_->currentItem()->data(Qt::UserRole).toInt(),
                                           values))
                refreshSupplies(false);
        });
    }
    auto supplyHelp = new QLabel("Changes apply immediately with Undo. These slots are edited "
                                 "directly; quantities in stored inventory stay unchanged.");
    supplyHelp->setWordWrap(true);
    suppliesLayout->addWidget(supplyHelp, 6, 0, 1, 4);
    body->addWidget(suppliesGroup);
    auto detailArea = new QScrollArea;
    detailArea->setObjectName("saveUnitDetailScroll");
    detailArea->setWidgetResizable(true);
    detailArea->setWidget(detail);
    split->addWidget(detailArea);
    split->setStretchFactor(1, 2);
    connect(units_, &QListWidget::currentRowChanged, this, [this] {
        if (!loading_ && editor_->hasPendingUnitEdits()) {
            const QSignalBlocker guard(units_);
            for (int i = 0; i < units_->count(); ++i)
                if (units_->item(i)->data(Qt::UserRole).toInt() == shownUnit_)
                    units_->setCurrentRow(i);
            return;
        }
        showUnit();
    });
    connect(members_, &QListWidget::currentRowChanged, this, [this] { drawFormation(); });
    connect(apply_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->editUnit(units_->currentItem()->data(Qt::UserRole).toInt(), cells_,
                              unity_->value(), tactic_->currentData().toInt());
    });
    connect(targetUnit_, &QComboBox::currentIndexChanged, this, [this] { refreshSwapTargets(); });
    connect(targetMember_, &QComboBox::currentIndexChanged, this, [this] { refreshSwapTargets(); });
    connect(unity_, &QSpinBox::valueChanged, this, [this] { refreshSwapTargets(); });
    connect(tactic_, &QComboBox::currentIndexChanged, this, [this] { refreshSwapTargets(); });
    connect(swap_, &QPushButton::clicked, this, [this] {
        if (!units_->currentItem())
            return;
        const int target = targetMember_->currentData().toInt();
        if (target < 0)
            editor_->transferUnitMember(units_->currentItem()->data(Qt::UserRole).toInt(),
                                        members_->currentRow(), targetUnit_->currentData().toInt(),
                                        -target - 1);
        else
            editor_->swapUnitMembers(units_->currentItem()->data(Qt::UserRole).toInt(),
                                     members_->currentRow(), targetUnit_->currentData().toInt(),
                                     target);
    });
    connect(toReserve_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->moveToReserve(units_->currentItem()->data(Qt::UserRole).toInt(),
                                   members_->currentRow());
    });
    connect(fromReserve_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->addFromReserve(reserveMember_->currentData().toInt(),
                                    units_->currentItem()->data(Qt::UserRole).toInt(),
                                    reserveCell_->currentData().toInt());
    });
    connect(editor_, &SaveEditor::refreshed, this, [this] { refresh(); });
    refresh();
}
void SaveUnitEditor::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    refreshSwapTargets(); // Project metadata/capacity may have changed in another workspace.
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
    shownUnit_ = -1;
    for (const auto &label : {"Autonomous", "Attack Strongest", "Attack Leader", "Attack Weakest"})
        tactic_->addItem(label, tactic_->count());
    if (editor_->document() && units_->currentItem()) {
        const int index = units_->currentItem()->data(Qt::UserRole).toInt();
        for (const auto &unit : editor_->document()->units()) {
            if (unit.index != index)
                continue;
            shownUnit_ = index;
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
    note_->setText(editable
                       ? "Edit formations, Unity, tactics and carried items. Move ordinary "
                         "nonleaders between squads or "
                         "reserve. Membership changes reset affected squads' Unity and recalculate "
                         "movement type."
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
    refreshSwapTargets();
}
void SaveUnitEditor::refreshSwapTargets() {
    if (loading_)
        return;
    const QSignalBlocker unitGuard(targetUnit_), memberGuard(targetMember_);
    const QSignalBlocker reserveGuard(reserveMember_), cellGuard(reserveCell_);
    const int oldUnit = targetUnit_->currentData().toInt(),
              oldMember = targetMember_->currentData().toInt();
    targetUnit_->clear();
    targetMember_->clear();
    const int oldReserve = reserveMember_->currentData().toInt(),
              oldCell = reserveCell_->currentData().toInt();
    reserveMember_->clear();
    reserveCell_->clear();
    bool ready = false, pending = false;
    bool acceptsReserve = false;
    if (editor_->document() && units_->currentItem()) {
        const auto &doc = *editor_->document();
        const int selected = units_->currentItem()->data(Qt::UserRole).toInt();
        const auto all = doc.units();
        for (int id : doc.reserveMembers())
            reserveMember_->addItem(memberName(id), id);
        for (const auto &unit : all) {
            if (unit.index == selected) {
                pending = cells_ != unit.cells || unity_->value() != unit.unity ||
                          tactic_->currentData().toInt() != unit.tactic;
                QSet<int> occupied;
                bool empty = false;
                for (int i = 0; i < 5; ++i) {
                    if (unit.members[i])
                        occupied << unit.cells[i];
                    else if (i > 0)
                        empty = true;
                }
                if (empty)
                    for (int cell = 0; cell < 9; ++cell)
                        if (!occupied.contains(cell))
                            reserveCell_->addItem(QString("Cell %1").arg(cell + 1), cell);
                acceptsReserve = doc.canEditUnits() && empty;
                continue;
            }
            bool eligible = false;
            for (int i = 1; i < 5; ++i)
                eligible |= unit.members[i] < 100;
            if (eligible)
                targetUnit_->addItem(
                    QString("Squad %1 · %2").arg(unit.index + 1).arg(memberName(unit.members[0])),
                    unit.index);
        }
        if (targetUnit_->findData(oldUnit) >= 0)
            targetUnit_->setCurrentIndex(targetUnit_->findData(oldUnit));
        if (targetUnit_->currentIndex() >= 0)
            for (const auto &unit : all)
                if (unit.index == targetUnit_->currentData().toInt()) {
                    bool empty = false;
                    QSet<int> occupied;
                    for (int i = 0; i < 5; ++i) {
                        if (unit.members[i])
                            occupied << unit.cells[i];
                        if (i > 0 && !unit.members[i])
                            empty = true;
                        if (i > 0 && unit.members[i] > 0 && unit.members[i] < 100)
                            targetMember_->addItem(memberName(unit.members[i]), i);
                    }
                    if (empty)
                        for (int cell = 0; cell < 9; ++cell)
                            if (!occupied.contains(cell))
                                targetMember_->addItem(QString("Empty cell %1").arg(cell + 1),
                                                       -cell - 1);
                }
        if (targetMember_->findData(oldMember) >= 0)
            targetMember_->setCurrentIndex(targetMember_->findData(oldMember));
        const int member = members_->currentRow();
        ready = doc.canEditUnits() && member > 0 && member < ids_.size() && ids_[member] > 0 &&
                ids_[member] < 100;
    }
    editor_->setUnitDraftPending(pending);
    units_->setEnabled(!pending);
    discard_->setEnabled(pending);
    draftNote_->setVisible(pending);
    refreshSupplies(pending);
    const bool enabled = ready && !pending && targetMember_->currentIndex() >= 0;
    swap_->setText(targetMember_->currentData().toInt() < 0 ? "Move selected member"
                                                            : "Swap selected members");
    swap_->setEnabled(enabled);
    targetUnit_->setEnabled(ready && !pending);
    targetMember_->setEnabled(ready && !pending);
    if (reserveMember_->findData(oldReserve) >= 0)
        reserveMember_->setCurrentIndex(reserveMember_->findData(oldReserve));
    if (reserveCell_->findData(oldCell) >= 0)
        reserveCell_->setCurrentIndex(reserveCell_->findData(oldCell));
    toReserve_->setEnabled(ready && !pending);
    const bool canAdd = acceptsReserve && !pending && reserveMember_->currentIndex() >= 0 &&
                        reserveCell_->currentIndex() >= 0;
    fromReserve_->setEnabled(canAdd);
    reserveMember_->setEnabled(canAdd);
    reserveCell_->setEnabled(canAdd);
    toReserve_->setToolTip(
        pending ? "Apply squad edits before moving members"
                : "Move the selected ordinary nonleader to reserve; carried items must still fit");
    fromReserve_->setToolTip(
        pending
            ? "Apply squad edits before moving members"
            : "Add the selected reserve character to an empty cell; formation limits still apply");
    swap_->setToolTip(pending
                          ? "Apply squad edits before moving members"
                          : "Move or swap ordinary nonleader members and reset both squads' Unity");
}
void SaveUnitEditor::refreshSupplies(bool pending) {
    QVector<int> values(10, 0);
    bool editable = false;
    int capacity = 0;
    int selected = -1;
    QString reason;
    if (editor_->document() && units_->currentItem()) {
        const int index = units_->currentItem()->data(Qt::UserRole).toInt();
        selected = index;
        for (const auto &unit : editor_->document()->units())
            if (unit.index == index) {
                values = unit.supplies;
                editable = editor_->document()->canEditUnits();
                try {
                    capacity = editor_->document()->unitCarryingCapacity(
                        index, editor_->project()->classData());
                } catch (const Error &e) {
                    editable = false;
                    reason = QString::fromUtf8(e.what());
                }
            }
    }
    const auto allowed = SaveDocument::carryableItems(*editor_->project());
    for (int i = 0; i < supplies_.size(); ++i) {
        auto box = supplies_[i];
        const QSignalBlocker guard(box);
        box->clear();
        box->addItem("Empty", 0);
        for (int id : allowed)
            if (selected >= 0 && id <= editor_->document()->maximumUnitSupply(selected, i))
                box->addItem(editor_->project()->record(Table::Consumables, id).name.simplified(),
                             id);
        if (box->findData(values[i]) < 0)
            box->addItem(QString("Keep existing item %1").arg(values[i]), values[i]);
        box->setCurrentIndex(box->findData(values[i]));
        box->setEnabled(editable && !pending);
        box->setToolTip(pending ? "Apply squad changes before editing carried items" : reason);
    }
    const int used =
        int(std::count_if(values.begin(), values.end(), [](int id) { return id != 0; }));
    capacity_->setText(reason.isEmpty()
                           ? QString("%1 carried · capacity %2 of 10 slots").arg(used).arg(capacity)
                           : reason);
}
} // namespace lordly
