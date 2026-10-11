#include "app/view_position.h"
#include "app/save_unit_editor.h"
#include "app/formation_board.h"
#include "app/save_editor.h"
#include "app/save_army_drag.h"
#include <QDrag>
#include <QDragEnterEvent>
#include <QComboBox>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTabWidget>
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
namespace {
class ArmyMemberList : public QListWidget {
  public:
    std::function<void(QListWidgetItem *)> dragMember;
  protected:
    void startDrag(Qt::DropActions) override {
        if (currentItem() && dragMember)
            dragMember(currentItem());
    }
};
class ArmyList final : public ArmyMemberList {
  public:
    explicit ArmyList(const QImage &floor) : floor_(floor.isNull() ? QPixmap{} : QPixmap::fromImage(floor.scaled(128,128,Qt::IgnoreAspectRatio,Qt::FastTransformation))) {
        setStyleSheet("QListWidget {background:transparent;border:2px solid #79672d;}"
                      "QListWidget::item {background:transparent;color:#fff2d1;}"
                      "QListWidget::item:selected {background:#715322;color:#fff2d1;}");
        setViewMode(QListView::IconMode); setResizeMode(QListView::Adjust);
        setMovement(QListView::Static); setSelectionMode(QAbstractItemView::SingleSelection);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
  protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter p(viewport());
        p.fillRect(viewport()->rect(), QColor("#222730"));
        if (!floor_.isNull()) p.drawTiledPixmap(viewport()->rect(), floor_);
        p.fillRect(viewport()->rect(), QColor(0,0,0,95));
        p.end();
        QListWidget::paintEvent(event);
    }
  private:
    QPixmap floor_;
};
class ArmyUnitDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {return {220,206};}
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save();
        const auto image = index.data(Qt::UserRole+1).value<QImage>();
        p->drawImage(option.rect.topLeft()+QPoint(6,0), image);
        const bool selected = option.state & QStyle::State_Selected;
        const QRect name(option.rect.left()+9,option.rect.bottom()-37,202,34);
        p->setPen(QPen(selected ? QColor("#fff291") : QColor("#a88d45"),selected ? 2 : 1));
        p->setBrush(selected ? QColor("#e7ce85") : QColor("#e2d7ac"));
        p->drawRoundedRect(name,2,2);
        p->setPen(QColor("#292414"));
        p->drawText(name.adjusted(5,1,-5,-1),Qt::AlignCenter|Qt::TextWordWrap,index.data().toString());
        if (selected) {
            p->setBrush(QColor("#fff291"));
            p->drawPolygon(QPolygon{option.rect.topLeft()+QPoint(14,42),option.rect.topLeft()+QPoint(26,48),option.rect.topLeft()+QPoint(14,54)});
        }
        p->restore();
    }
};
}

SaveUnitEditor::SaveUnitEditor(SaveEditor *editor) : QWidget(editor), editor_(editor) {
    setObjectName("saveSquads");
    auto layout = new QVBoxLayout(this);
    note_ = new QLabel;
    note_->setWordWrap(true);
    layout->addWidget(note_);
    armyCounts_ = new QLabel;
    armyCounts_->setObjectName("saveArmyCounts");
    armyCounts_->setAlignment(Qt::AlignCenter);
    armyCounts_->setMinimumHeight(48);
    armyCounts_->setStyleSheet("background:#ded6a6;color:#302c14;border:3px ridge #a4a05b;padding:5px;font-size:16px;font-weight:bold;");
    layout->addWidget(armyCounts_);
    auto creation = new QHBoxLayout;
    addCharacter_ = new QPushButton("Add character…");
    addCharacter_->setObjectName("saveArmyAddCharacter");
    recruitSquad_ = new QPushButton("Add squad…");
    recruitSquad_->setObjectName("saveArmyRecruitSquad");
    creation->addWidget(addCharacter_); creation->addWidget(recruitSquad_);
    creation->addSpacing(12);
    creation->addWidget(new QLabel("Reserve leader"));
    newLeader_ = new QComboBox;
    newLeader_->setObjectName("saveNewUnitLeader");
    creation->addWidget(newLeader_, 1);
    creation->addWidget(new QLabel("Formation cell"));
    newCell_ = new QComboBox;
    newCell_->setObjectName("saveNewUnitCell");
    for (int cell = 0; cell < 9; ++cell)
        newCell_->addItem(QString::number(cell + 1), cell);
    newCell_->setCurrentIndex(4);
    creation->addWidget(newCell_);
    create_ = new QPushButton("Form squad from reserve");
    create_->setObjectName("saveCreateUnit");
    creation->addWidget(create_);
    layout->addLayout(creation);
    creationNote_ = new QLabel;
    creationNote_->setObjectName("saveArmyCreationNote");
    creationNote_->setWordWrap(true); layout->addWidget(creationNote_);
    connect(addCharacter_, &QPushButton::clicked, this, [this] {
        const int id=editor_->promptAddCharacter();
        if (id>=0) {
            for (int row=0;row<reserves_->count();++row)
                if (reserves_->item(row)->data(Qt::UserRole).toInt()==id) reserves_->setCurrentRow(row);
            if (const int leader = newLeader_->findData(id); leader >= 0)
                newLeader_->setCurrentIndex(leader);
            detailTabs_->setCurrentIndex(1);
        }
    });
    connect(recruitSquad_, &QPushButton::clicked, this, [this] {
        const int id=editor_->promptAddCharacter(true);
        if (id>=0) selectLeader(id);
    });
    connect(create_, &QPushButton::clicked, this, [this] {
        const int leader = newLeader_->currentData().toInt();
        if (editor_->createUnit(leader, newCell_->currentData().toInt()))
            for (int row = 0; row < units_->count(); ++row)
                for (const auto &unit : editor_->document()->units())
                    if (unit.index == units_->item(row)->data(Qt::UserRole).toInt() &&
                        unit.members[0] == leader)
                        units_->setCurrentRow(row);
    });
    auto split = new QSplitter;
    layout->addWidget(split, 1);
    QImage floor;
    try {floor=ArmyArtwork::floorImage(editor_->project()->rom());} catch (const Error &) {}
    auto roster = new QWidget;
    auto rosterLayout = new QVBoxLayout(roster);
    rosterLayout->setContentsMargins(0,0,0,0);
    units_ = new ArmyList(floor);
    units_->setObjectName("saveUnitList");
    units_->setItemDelegate(new ArmyUnitDelegate(units_));
    units_->setGridSize({220,206}); units_->setMinimumWidth(250);
    rosterLayout->addWidget(units_,1);
    auto reserveTitle = new QLabel("RESERVE CHARACTERS · drag to or from a squad · double-click to edit");
    reserveTitle->setWordWrap(true);
    reserveTitle->setStyleSheet("background:#ded6a6;border:1px solid #a4a05b;padding:4px;font-weight:bold;");
    rosterLayout->addWidget(reserveTitle);
    reserves_ = new ArmyList(floor);
    reserves_->setObjectName("saveArmyReserve");
    reserves_->setIconSize({48,56}); reserves_->setGridSize({112,90});
    reserves_->setFixedHeight(110);
    rosterLayout->addWidget(reserves_);
    funds_ = new QLabel;
    funds_->setObjectName("saveArmyFunds");
    funds_->setStyleSheet("background:#ded6a6;color:#302c14;border:3px ridge #a4a05b;padding:7px;font-size:16px;font-weight:bold;");
    rosterLayout->addWidget(funds_);
    split->addWidget(roster);
    auto detail = new QWidget;
    auto body = new QVBoxLayout(detail);
    members_ = new ArmyMemberList;
    members_->setObjectName("saveUnitMembers");
    members_->setViewMode(QListView::IconMode);
    members_->setResizeMode(QListView::Adjust);
    members_->setMovement(QListView::Static);
    members_->setGridSize({72,86});
    members_->setWrapping(false);
    members_->setFixedHeight(100);
    members_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->addWidget(members_);
    members_->setIconSize({48,56});
    detailTabs_ = new QTabWidget;
    detailTabs_->setObjectName("saveArmyDetails");
    body->addWidget(detailTabs_,1);
    auto makePage=[&](const char *title) {
        auto page=new QWidget;
        auto contents=new QVBoxLayout(page);
        contents->setAlignment(Qt::AlignTop);
        detailTabs_->addTab(page,title);
        return contents;
    };
    auto formationBody=makePage("Formation");
    auto transfersBody=makePage("Members");
    auto legionBody=makePage("Legion");
    auto itemsBody=makePage("Items");

    legion_ = new QLabel;
    legion_->setObjectName("saveUnitLegion");
    legionBody->addWidget(legion_);
    createLegion_ = new QPushButton("Create a legion from this command squad");
    createLegion_->setObjectName("saveCreateLegion");
    legionBody->addWidget(createLegion_);
    connect(createLegion_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->createLegion(units_->currentItem()->data(Qt::UserRole).toInt());
    });
    auto legionRow = new QHBoxLayout;
    legionRow->addWidget(new QLabel("Assign this squad to"));
    legionTarget_ = new QComboBox;
    legionTarget_->setObjectName("saveLegionTarget");
    legionRow->addWidget(legionTarget_, 1);
    joinLegion_ = new QPushButton("Join legion");
    joinLegion_->setObjectName("saveJoinLegion");
    legionRow->addWidget(joinLegion_);
    legionBody->addLayout(legionRow);
    connect(joinLegion_, &QPushButton::clicked, this, [this] {
        if (legionTarget_->currentIndex() >= 0 && units_->currentItem())
            editor_->joinLegion(legionTarget_->currentData().toInt(),
                                units_->currentItem()->data(Qt::UserRole).toInt());
    });
    detachLegion_ = new QPushButton("Detach this squad from its legion");
    detachLegion_->setObjectName("saveDetachLegionUnit");
    legionBody->addWidget(detachLegion_);
    connect(detachLegion_, &QPushButton::clicked, this, [this] {
        if (shownLegion_ >= 0 && units_->currentItem())
            editor_->detachLegionUnit(shownLegion_,
                                      units_->currentItem()->data(Qt::UserRole).toInt());
    });
    disbandLegion_ = new QPushButton("Disband legion, keep its squads");
    disbandLegion_->setObjectName("saveDisbandLegion");
    legionBody->addWidget(disbandLegion_);
    connect(disbandLegion_, &QPushButton::clicked, this, [this] {
        if (shownLegion_ >= 0)
            editor_->disbandLegion(shownLegion_);
    });
    leader_ = new QPushButton("Make selected member the leader");
    leader_->setObjectName("saveChangeLeader");
    transfersBody->addWidget(leader_);
    disband_ = new QPushButton("Disband this squad to reserve");
    disband_->setObjectName("saveDisbandUnit");
    transfersBody->addWidget(disband_);
    connect(disband_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->disbandUnit(units_->currentItem()->data(Qt::UserRole).toInt());
    });
    connect(leader_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->changeUnitLeader(units_->currentItem()->data(Qt::UserRole).toInt(),
                                      members_->currentRow());
    });
    auto help =
        new QLabel("Drag a character to move or swap them. Select a member above to place them by clicking a cell.");
    help->setWordWrap(true);
    formationBody->addWidget(help);
    board_ = new FormationBoard(editor_->project(), "saveUnitCell", true);
    board_->setPixelScale(3);
    board_->setExternalDragEnabled(true);
    board_->setAcceptDrops(true);
    board_->installEventFilter(this);
    for (auto list : {units_, reserves_}) {
        list->setAcceptDrops(true);
        list->viewport()->setAcceptDrops(true);
        list->viewport()->installEventFilter(this);
    }
    for (auto list : {members_, reserves_})
        list->setDragEnabled(true);
    static_cast<ArmyMemberList *>(members_)->dragMember = [this](QListWidgetItem *item) {
        const int member = members_->row(item);
        if (member >= 0 && member < ids_.size())
            startMemberDrag(shownUnit_, member, ids_[member]);
    };
    static_cast<ArmyMemberList *>(reserves_)->dragMember = [this](QListWidgetItem *item) {
        startMemberDrag(-1, -1, item->data(Qt::UserRole).toInt());
    };
    connect(board_, &FormationBoard::cellDraggedOutside, this, [this](int cell) {
        for (int member = 0; member < ids_.size(); ++member)
            if (ids_[member] && cells_[member] == cell) {
                startMemberDrag(shownUnit_, member, ids_[member]);
                break;
            }
    });
    for (int cell = 0; cell < 9; ++cell) {
        auto button = board_->cell(cell);
        grid_.append(button);
        connect(button, &QPushButton::clicked, this, [this, cell] {
            const int member = members_->currentRow();
            if (loading_ || member < 0 || member >= ids_.size() || !ids_[member]) return;
            const int from = cells_[member];
            for (int i = 0; i < ids_.size(); ++i)
                if (i != member && ids_[i] && cells_[i] == cell) cells_[i] = from;
            cells_[member] = cell;
            drawFormation();
        });
    }
    connect(board_, &FormationBoard::cellDragStarted, this, [this](int cell) {
        for (int i = 0; i < ids_.size(); ++i)
            if (ids_[i] && cells_[i] == cell) {members_->setCurrentRow(i);break;}
    });
    connect(board_, &FormationBoard::cellMoved, this, [this](int, int to) {grid_[to]->click();});
    formationBody->addWidget(board_, 0, Qt::AlignHCenter);
    auto form = new QFormLayout;
    unity_ = new QSpinBox;
    unity_->setObjectName("saveUnitUnity");
    unity_->setRange(0, 100);
    tactic_ = new QComboBox;
    tactic_->setObjectName("saveUnitTactic");
    form->addRow("Unity", unity_);
    form->addRow("Tactic", tactic_);
    formationBody->addLayout(form);
    apply_ = new QPushButton("Apply squad changes");
    apply_->setObjectName("saveUnitApply");
    auto draftActions = new QHBoxLayout;
    draftActions->addWidget(apply_);
    discard_ = new QPushButton("Discard squad changes");
    discard_->setObjectName("saveUnitDiscard");
    draftActions->addWidget(discard_);
    formationBody->addLayout(draftActions);
    draftNote_ = new QLabel("Unapplied changes. Apply or discard them before changing squads, "
                            "save slots, other save fields, exporting, or using Undo.");
    draftNote_->setObjectName("saveUnitDraftNote");
    draftNote_->setWordWrap(true);
    formationBody->addWidget(draftNote_);
    connect(discard_, &QPushButton::clicked, this, [this] { showUnit(); });
    auto transfers = new QFormLayout;
    targetUnit_ = new QComboBox;
    targetUnit_->setObjectName("saveSwapUnit");
    targetMember_ = new QComboBox;
    targetMember_->setObjectName("saveSwapMember");
    transfers->addRow("Destination squad", targetUnit_);
    transfers->addRow("Member or empty cell", targetMember_);
    transfersBody->addLayout(transfers);
    swap_ = new QPushButton("Swap selected members");
    swap_->setObjectName("saveSwapApply");
    transfersBody->addWidget(swap_);
    toReserve_ = new QPushButton("Move selected member to reserve");
    toReserve_->setObjectName("saveMoveToReserve");
    transfersBody->addWidget(toReserve_);
    auto reserve = new QFormLayout;
    reserveMember_ = new QComboBox;
    reserveMember_->setObjectName("saveReserveMember");
    reserveCell_ = new QComboBox;
    reserveCell_->setObjectName("saveReserveCell");
    reserve->addRow("Reserve character", reserveMember_);
    reserve->addRow("Empty cell in this squad", reserveCell_);
    transfersBody->addLayout(reserve);
    fromReserve_ = new QPushButton("Add reserve character to this squad");
    fromReserve_->setObjectName("saveAddFromReserve");
    transfersBody->addWidget(fromReserve_);
    soldierPool_ = new QLabel;
    soldierPool_->setObjectName("saveSoldierPool");
    transfersBody->addWidget(soldierPool_);
    addSoldier_ = new QPushButton("Create Soldier group in this squad");
    addSoldier_->setObjectName("saveAddSoldierGroup");
    transfersBody->addWidget(addSoldier_);
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
        choice->setMinimumContentsLength(12);
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
    itemsBody->addWidget(suppliesGroup);
    auto detailArea = new QScrollArea;
    detailArea->setObjectName("saveUnitDetailScroll");
    detailArea->setWidgetResizable(true);
    detailArea->setMinimumWidth(440);
    detailArea->setWidget(detail);
    split->addWidget(detailArea);
    split->setStretchFactor(0, 3); split->setStretchFactor(1, 2);
    split->setSizes({660,480});
    connect(reserves_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row<0 || loading_) return;
        const int id=reserves_->item(row)->data(Qt::UserRole).toInt();
        if (const int leader = newLeader_->findData(id); leader >= 0)
            newLeader_->setCurrentIndex(leader);
        reserveMember_->setCurrentIndex(reserveMember_->findData(id));
    });
    connect(reserves_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        editor_->selectCharacter(item->data(Qt::UserRole).toInt());
    });
    connect(members_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const int row=members_->row(item);
        if (row>=0 && row<ids_.size() && ids_[row]>0 && ids_[row]<100) editor_->selectCharacter(ids_[row]);
    });
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
    connect(addSoldier_, &QPushButton::clicked, this, [this] {
        if (units_->currentItem())
            editor_->addSoldierGroup(units_->currentItem()->data(Qt::UserRole).toInt(),
                                     reserveCell_->currentData().toInt());
    });
    connect(editor_, &SaveEditor::refreshed, this, [this] { refresh(); });
    refresh();
}
void SaveUnitEditor::startMemberDrag(int unit, int member, int id) {
    if (!editor_->document() || !editor_->document()->canEditUnits() ||
        editor_->hasPendingUnitEdits() || id <= 0)
        return;
    auto drag = new QDrag(this);
    drag->setMimeData(new SaveArmyDrag(this, unit, member, id));
    const auto sprite = memberSprite(id);
    QPixmap preview(160, 96);
    preview.fill(palette().color(QPalette::Base));
    QPainter painter(&preview);
    if (!sprite.isNull()) {
        const auto scaled = sprite.scaled(64, 64, Qt::KeepAspectRatio, Qt::FastTransformation);
        painter.drawImage(QPoint((160 - scaled.width()) / 2, 0), scaled);
    }
    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(QRect(2, 64, 156, 30), Qt::AlignCenter, memberName(id));
    painter.end();
    drag->setPixmap(preview);
    drag->setHotSpot({80, 32});
    drag->exec(Qt::MoveAction);
    board_->setDropCell(-1);
    drag->deleteLater();
}
bool SaveUnitEditor::dropMember(const SaveArmyDrag *drag, QObject *target, QPoint position,
                                bool apply) {
    if (!drag || drag->owner != this || !editor_->document() ||
        !editor_->document()->canEditUnits() || editor_->hasPendingUnitEdits())
        return false;
    const auto &doc = *editor_->document();
    const auto all = doc.units();
    if (drag->unit < 0) {
        if (!doc.reserveMembers().contains(drag->id))
            return false;
    } else {
        const auto source = std::find_if(all.begin(), all.end(),
            [&](const auto &unit) { return unit.index == drag->unit; });
        if (source == all.end() || drag->member < 0 || drag->member >= source->members.size() ||
            source->members[drag->member] != drag->id)
            return false;
    }
    try {
        if (target == reserves_->viewport()) {
            if (drag->unit < 0)
                return false;
            doc.planToReserve(drag->unit, drag->member, editor_->project()->classData());
            return !apply || editor_->moveToReserve(drag->unit, drag->member);
        }
        int destination = shownUnit_;
        QVector<int> cells;
        if (target == board_) {
            const int cell = board_->cellAt(position);
            if (cell < 0 || !grid_[cell]->isEnabled())
                return false;
            cells.append(cell);
            if (drag->unit == destination) {
                if (apply) {
                    members_->setCurrentRow(drag->member);
                    grid_[cell]->click();
                }
                return true;
            }
        } else if (target == units_->viewport()) {
            const auto item = units_->itemAt(position);
            if (!item)
                return false;
            destination = item->data(Qt::UserRole).toInt();
            if (drag->unit == destination)
                return false;
            for (int cell = 0; cell < 9; ++cell)
                cells.append(cell);
        } else {
            return false;
        }
        QString rejection;
        for (int cell : cells) {
            try {
                if (drag->unit < 0)
                    doc.planFromReserve(drag->id, destination, cell, editor_->project()->classData());
                else
                    doc.planUnitTransfer(drag->unit, drag->member, destination, cell,
                                         editor_->project()->classData());
            } catch (const Error &error) {
                rejection = QString::fromUtf8(error.what());
                continue;
            }
            if (!apply)
                return true;
            const bool moved = drag->unit < 0
                ? editor_->addFromReserve(drag->id, destination, cell)
                : editor_->transferUnitMember(drag->unit, drag->member, destination, cell);
            if (moved)
                for (int row = 0; row < units_->count(); ++row)
                    if (units_->item(row)->data(Qt::UserRole).toInt() == destination) {
                        units_->setCurrentRow(row);
                        break;
                    }
            return moved;
        }
        if (apply && !rejection.isEmpty())
            note_->setText(rejection);
    } catch (const Error &error) {
        if (apply)
            note_->setText(QString::fromUtf8(error.what()));
    }
    return false;
}
bool SaveUnitEditor::eventFilter(QObject *target, QEvent *event) {
    if (event->type() == QEvent::DragLeave) {
        board_->setDropCell(-1);
        event->accept();
        return true;
    }
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove ||
        event->type() == QEvent::Drop) {
        auto drop = static_cast<QDropEvent *>(event);
        const auto drag = dynamic_cast<const SaveArmyDrag *>(drop->mimeData());
        const bool entering = event->type() == QEvent::DragEnter;
        const bool valid = (drop->possibleActions() & Qt::MoveAction) &&
            (entering ? drag && drag->owner == this && editor_->document() &&
                            editor_->document()->canEditUnits() && !editor_->hasPendingUnitEdits()
                      : dropMember(drag, target, drop->position().toPoint(), event->type() == QEvent::Drop));
        if (valid) {
            drop->setDropAction(Qt::MoveAction);
            drop->accept();
        } else {
            drop->ignore();
        }
        board_->setDropCell(valid && target == board_ && event->type() == QEvent::DragMove
                                ? board_->cellAt(drop->position().toPoint()) : -1);
        return true;
    }
    return QWidget::eventFilter(target, event);
}
void SaveUnitEditor::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // Include overview/reserve sprites when returning from class or artwork edits.
    if (editor_->hasPendingUnitEdits()) drawFormation();
    else refresh();
}
QString SaveUnitEditor::memberName(int id) const {
    if (!id)
        return "Empty";
    if (id >= 220)
        return QString("Unknown member %1").arg(id);
    if (id >= 100)
        return QString("Soldier group %1").arg(id - 99);
    try {
        return editor_->document()->value(SaveTable::Characters, id, "name").toString();
    } catch (const Error &) {
        return QString("Missing character %1").arg(id);
    }
}
QImage SaveUnitEditor::memberSprite(int id) const {
    if (id <= 0 || id >= 220) return {};
    try {
        const int classId = id>=100 ? 0 : editor_->document()->value(SaveTable::Characters,id,"class").toInt();
        editor_->project()->loadArmyArtwork();
        const auto classes=editor_->project()->classData();
        if (classId<0 || classId>=classes.entries().size()) return {};
        const auto &asset=classes.entries()[classId];
        return editor_->project()->armyArtwork()->image(ArmyArtwork::classLane(asset)*2,ArmyArtwork::classModel(asset));
    } catch (const Error &) {return {};}
}
void SaveUnitEditor::selectLeader(int id) {
    if (!editor_->document()) return;
    for (const auto &unit : editor_->document()->units())
        if (unit.members[0]==id)
            for (int row=0;row<units_->count();++row)
                if (units_->item(row)->data(Qt::UserRole).toInt()==unit.index) {
                    units_->setCurrentRow(row); units_->scrollToItem(units_->currentItem()); return;
                }
}
void SaveUnitEditor::refresh() {
    const ViewPosition keepView(this);
    loading_ = true;
    const int prior = units_->currentItem() ? units_->currentItem()->data(Qt::UserRole).toInt() : -1;
    const int reservePrior=reserves_->currentItem() ? reserves_->currentItem()->data(Qt::UserRole).toInt() : -1;
    units_->clear(); reserves_->clear();
    int soldiers=0, characters=0;
    quint32 funds=0;
    if (editor_->document()) {
        const auto &doc=*editor_->document();
        characters=doc.rows(SaveTable::Characters).size();
        soldiers=doc.reserveSoldiers();
        funds=doc.value(SaveTable::Game,0,"goth").toUInt();
        FormationBoard thumbnail(editor_->project(),"armyThumbnail",true);
        for (const auto &unit : doc.units()) {
            thumbnail.clearUnits();
            for (int i=0;i<unit.members.size();++i) if (unit.members[i]) {
                const int member=unit.members[i];
                if (member>=100 && member<220) soldiers+=doc.soldiersInGroup(member);
                int classId=-1;
                try {classId=member>=100 && member<220 ? 0 : doc.value(SaveTable::Characters,member,"class").toInt();}
                catch (const Error &) {}
                thumbnail.setUnit(unit.cells[i],classId,memberName(member));
            }
            auto item=new QListWidgetItem(QString("Squad %1 · %2").arg(unit.label).arg(memberName(unit.members[0])),units_);
            item->setData(Qt::UserRole,unit.index);
            item->setData(Qt::UserRole+1,thumbnail.overviewImage());
            item->setToolTip(item->text()+QString("\nUnity %1 · double-click a member to edit their character").arg(unit.unity));
            if (unit.index==prior) units_->setCurrentItem(item);
        }
        for (int id : doc.reserveMembers()) {
            const auto image=memberSprite(id);
            const auto icon=image.isNull() ? QIcon{} : QIcon(QPixmap::fromImage(image.scaled(image.size()*2,Qt::IgnoreAspectRatio,Qt::FastTransformation)));
            auto item=new QListWidgetItem(icon,memberName(id),reserves_);
            item->setData(Qt::UserRole,id); item->setToolTip(memberName(id));
            if (id==reservePrior) reserves_->setCurrentItem(item);
        }
    }
    armyCounts_->setText(QString("SOLDIERS   %1       |       CHARACTERS   %2       |       UNITS   %3 / 30")
                        .arg(soldiers).arg(characters).arg(units_->count()));
    funds_->setText(QString("WAR FUNDS     %1 Goth").arg(QLocale().toString(funds)));
    if (!units_->currentItem() && units_->count()) units_->setCurrentRow(0);
    loading_=false;
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
                item->setToolTip(item->text());
                if (ids_[i]) {
                    const auto image=memberSprite(ids_[i]);
                    if (!image.isNull())
                        item->setIcon(QIcon(QPixmap::fromImage(image.scaled(image.size()*2,Qt::IgnoreAspectRatio,Qt::FastTransformation))));
                }
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
    note_->setText(editable ? "Drag a member icon or formation sprite to Reserve. Drag a reserve character onto a formation cell or squad. Double-click a member to edit stats and equipment."
                            : editor_->document() ? "Viewing a runtime/suspend save. Use a regular battery-save slot to create or reorganize squads."
                                                  : "Open a save to view and manage your army.");
    apply_->setEnabled(editable && !ids_.isEmpty());
    unity_->setEnabled(apply_->isEnabled());
    tactic_->setEnabled(apply_->isEnabled());
    loading_ = false;
    drawFormation();
}
void SaveUnitEditor::drawFormation() {
    if (loading_)
        return;
    board_->clearUnits();
    for (int cell = 0; cell < 9; ++cell) {
        QStringList names;
        int classId = 0;
        bool selected = false;
        for (int member = 0; member < ids_.size(); ++member)
            if (ids_[member] && cells_[member] == cell) {
                names << memberName(ids_[member]) + (member == 0 ? " · Leader" : "");
                selected |= members_->currentRow() == member;
                if (ids_[member] < 100)
                    classId = editor_->document()->value(SaveTable::Characters, ids_[member], "class").toInt();
            }
        grid_[cell]->setText(QString("%1\n%2").arg(cell + 1).arg(names.join(" + ")));
        if (!names.isEmpty()) board_->setUnit(cell, classId, grid_[cell]->text(), selected, names.size()>1);
        grid_[cell]->setEnabled(apply_->isEnabled());
    }
    board_->finishUpdate();
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
    bool ready = false, returnReady = false, soldier = false, pending = false;
    int pool = 0, returning = 0;
    bool acceptsReserve = false;
    if (editor_->document() && units_->currentItem()) {
        const auto &doc = *editor_->document();
        const int selected = units_->currentItem()->data(Qt::UserRole).toInt();
        const auto all = doc.units();
        pool = doc.reserveSoldiers();
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
                eligible |= unit.members[i] < 220;
            if (eligible)
                targetUnit_->addItem(
                    QString("Squad %1 · %2").arg(unit.label).arg(memberName(unit.members[0])),
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
                        if (i > 0 && unit.members[i] > 0 && unit.members[i] < 220)
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
        returnReady = doc.canEditUnits() && member > 0 && member < ids_.size() &&
                      ids_[member] > 0 && ids_[member] < 220;
        soldier = returnReady && ids_[member] >= 100;
        ready = returnReady;
        if (soldier)
            returning = doc.soldiersInGroup(ids_[member]);
    }
    editor_->setUnitDraftPending(pending);
    units_->setEnabled(!pending);
    discard_->setEnabled(pending);
    draftNote_->setVisible(pending);
    refreshSupplies(pending);
    refreshCreation(pending);
    shownLegion_ = -1;
    bool follower = false;
    legion_->setText("Independent squad");
    if (editor_->document() && units_->currentItem())
        for (const auto &legion : editor_->document()->legions())
            if (legion.units.contains(units_->currentItem()->data(Qt::UserRole).toInt())) {
                shownLegion_ = legion.index;
                follower = legion.units[0] != units_->currentItem()->data(Qt::UserRole).toInt();
                const int count = int(std::count_if(legion.units.begin(), legion.units.end(),
                                                    [](int id) { return id != 255; }));
                legion_->setText(QString("Legion %1 · %2 squads").arg(legion.label).arg(count));
                break;
            }
    createLegion_->setEnabled(editor_->document() && units_->currentItem() && !pending &&
                              editor_->document()->legions().size() < 6 &&
                              editor_->document()
                                  ->newLegionCommanders(editor_->project()->classData())
                                  .contains(units_->currentItem()->data(Qt::UserRole).toInt()));
    createLegion_->setToolTip(
        pending ? "Apply squad edits before creating a legion"
                : "Starts a legion with this independent command squad. Add followers with Join "
                  "legion; "
                  "each follower requires one Soldier group in the command squad.");
    const int priorLegion = legionTarget_->currentData().toInt();
    legionTarget_->clear();
    if (editor_->document() && editor_->document()->canEditUnits() && shownLegion_ < 0)
        for (const auto &target : editor_->document()->legions()) {
            try {
                const int capacity = editor_->document()->legionCapacity(target.index);
                const int used = int(std::count_if(target.units.begin(), target.units.end(),
                                                   [](int id) { return id != 255; }));
                if (used < capacity)
                    legionTarget_->addItem(QString("Legion %1 (%2/%3 squads)")
                                               .arg(target.label)
                                               .arg(used)
                                               .arg(capacity),
                                           target.index);
            } catch (const Error &) {
                // Malformed legion membership cannot be a safe join target.
            }
        }
    const int targetRow = legionTarget_->findData(priorLegion);
    if (targetRow >= 0)
        legionTarget_->setCurrentIndex(targetRow);
    const bool canJoin = units_->currentItem() && legionTarget_->count() && !pending;
    legionTarget_->setEnabled(canJoin);
    joinLegion_->setEnabled(canJoin);
    joinLegion_->setToolTip(
        pending ? "Apply squad edits before changing legion membership"
                : "Assigns this independent squad to the first free follower position. "
                  "Each follower requires a Soldier group in the command squad.");
    detachLegion_->setEnabled(follower && editor_->document()->canEditUnits() && !pending);
    detachLegion_->setToolTip(
        pending ? "Apply squad edits before changing legion membership"
                : "Keeps this squad and its contents; the remaining legion retains its commander. "
                  "Use Disband legion to release the command squad.");
    disbandLegion_->setEnabled(shownLegion_ >= 0 && editor_->document()->canEditUnits() &&
                               !pending);
    disbandLegion_->setToolTip(pending
                                   ? "Apply squad edits before changing legion membership"
                                   : "Makes all squads independent, retaining their leaders, "
                                     "formations, Unity and supplies; Undo restores the legion");
    disband_->setEnabled(editor_->document() && editor_->document()->canEditUnits() &&
                         units_->currentItem() && !pending);
    disband_->setToolTip(
        pending ? "Apply squad edits before disbanding"
                : "Returns ordinary members and Soldiers to reserve and releases carried items "
                  "without changing total ownership; Undo restores the whole squad");
    const bool enabled = ready && !pending && targetMember_->currentIndex() >= 0;
    swap_->setText(targetMember_->currentData().toInt() < 0 ? "Move selected member"
                                                            : "Swap selected members");
    swap_->setEnabled(enabled);
    leader_->setEnabled(ready && !soldier && !pending);
    leader_->setToolTip(
        pending ? "Apply squad edits before changing the leader"
                : "Keeps formation positions, Unity and legion membership; updates both character "
                  "leader flags. "
                  "Fixed leaders and incompatible legion commanders cannot be replaced.");
    targetUnit_->setEnabled(ready && !pending);
    targetMember_->setEnabled(ready && !pending);
    if (reserveMember_->findData(oldReserve) >= 0)
        reserveMember_->setCurrentIndex(reserveMember_->findData(oldReserve));
    if (reserveCell_->findData(oldCell) >= 0)
        reserveCell_->setCurrentIndex(reserveCell_->findData(oldCell));
    toReserve_->setEnabled(returnReady && !pending);
    toReserve_->setText(soldier ? "Disband selected Soldier group"
                                : "Move selected member to reserve");
    soldierPool_->setText(QString("Soldiers in reserve: %1").arg(pool));
    const bool freeCell = acceptsReserve && !pending && reserveCell_->currentIndex() >= 0;
    addSoldier_->setEnabled(freeCell && pool > 0);
    addSoldier_->setText(QString("Create group from %1 reserve Soldiers").arg(std::min(3, pool)));
    addSoldier_->setToolTip(pending ? "Apply squad edits before creating a group"
                                    : "Uses the selected empty cell and the first free saved group "
                                      "slot. Unity is retained.");
    const bool canAdd = acceptsReserve && !pending && reserveMember_->currentIndex() >= 0 &&
                        reserveCell_->currentIndex() >= 0;
    fromReserve_->setEnabled(canAdd);
    reserveMember_->setEnabled(canAdd);
    reserveCell_->setEnabled(freeCell);
    toReserve_->setToolTip(
        pending ? "Apply squad edits before moving members"
        : soldier
            ? QString("Returns %1 Soldiers based on current HP, clears the group HP/state, "
                      "and retains Unity. Carried items must still fit.")
                  .arg(returning)
            : QString(
                  "Move the selected ordinary nonleader to reserve and reset squad Unity; carried items must still fit"));
    fromReserve_->setToolTip(
        pending
            ? "Apply squad edits before moving members"
            : "Add the selected reserve character to an empty cell and reset squad Unity; formation limits still apply");
    swap_->setToolTip(pending ? "Apply squad edits before moving members"
                              : "Move or swap nonleaders, retaining Unity and Soldier HP/state; "
                                "carried items must fit");
}
void SaveUnitEditor::refreshCreation(bool pending) {
    const QSignalBlocker guard(newLeader_);
    const int prior = newLeader_->currentData().toInt();
    newLeader_->clear();
    bool available = false;
    if (editor_->document() && editor_->document()->canEditUnits()) {
        const auto &doc = *editor_->document();
        for (int id : doc.newUnitLeaders(editor_->project()->classData()))
            newLeader_->addItem(memberName(id), id);
        available = doc.units().size() < 30 && newLeader_->count() > 0;
    }
    if (newLeader_->findData(prior) >= 0)
        newLeader_->setCurrentIndex(newLeader_->findData(prior));
    const bool editable=editor_->document() && editor_->document()->canEditUnits();
    const bool slot=editable && editor_->document()->freeCharacterSlot()>=0;
    const bool squadSlot=editable && editor_->document()->units().size()<30;
    addCharacter_->setEnabled(slot && !pending);
    recruitSquad_->setEnabled(slot && squadSlot && !pending);
    reserves_->setEnabled(!pending);
    creationNote_->setText(pending ? "Apply or discard formation edits before recruiting or forming another squad."
        : !editable ? "Character and squad creation requires a regular battery-save slot."
        : !squadSlot ? "All 30 squad slots are in use. Disband a squad to make room."
        : !newLeader_->count() ? "No eligible reserve leader. Add squad recruits a new leader, or add a character to your reserves."
        : "Add squad recruits a new leader. Form squad from reserve uses the selected existing character.");
    if (editable && !slot) creationNote_->setText(creationNote_->text()+" The character roster is full.");
    create_->setEnabled(available && !pending);
    newLeader_->setEnabled(available && !pending);
    newCell_->setEnabled(available && !pending);
    create_->setToolTip(
        pending ? "Apply squad edits before creating another squad"
                : "Uses an eligible reserve character and the first unused squad number; "
                  "starts with zero Unity, Autonomous tactics and no carried items");
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
            if (selected >= 0 && id <= editor_->document()->maximumUnitSupply(selected, i)) {
                int available = 0;
                try {
                    available = editor_->document()->consumableAvailable(id);
                } catch (const Error &) {
                    continue;
                }
                if (id == values[i] || available > 0)
                    box->addItem(
                        editor_->project()->record(Table::Consumables, id).name.simplified(), id);
            }
        if (box->findData(values[i]) < 0)
            box->addItem(QString("Keep existing item %1").arg(values[i]), values[i]);
        box->setCurrentIndex(box->findData(values[i]));
        box->setEnabled(editable && !pending);
        box->setToolTip(pending            ? "Apply squad changes before editing carried items"
                        : reason.isEmpty() ? "Choose an unassigned copy from owned inventory; "
                                             "removing a carried item makes it available again"
                                           : reason);
    }
    const int used =
        int(std::count_if(values.begin(), values.end(), [](int id) { return id != 0; }));
    capacity_->setText(reason.isEmpty()
                           ? QString("%1 carried · capacity %2 of 10 slots").arg(used).arg(capacity)
                           : reason);
}
} // namespace lordly
