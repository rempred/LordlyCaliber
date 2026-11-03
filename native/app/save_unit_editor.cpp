#include "app/view_position.h"
#include "app/save_unit_editor.h"
#include "app/save_character_card.h"
#include "app/item_picker.h"
#include "app/formation_board.h"
#include "app/save_editor.h"
#include "app/save_army_drag.h"
#include <QDrag>
#include <QDragEnterEvent>
#include <QMouseEvent>
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
    QPoint dragPosition() const { return pressPosition_; }
    QPoint doubleClickPosition() const { return doubleClickPosition_; }
  protected:
    void mousePressEvent(QMouseEvent *event) override {
        pressPosition_ = event->position().toPoint();
        QListWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        doubleClickPosition_ = event->position().toPoint();
        QListWidget::mouseDoubleClickEvent(event);
    }
    void startDrag(Qt::DropActions) override {
        if (currentItem() && dragMember)
            dragMember(currentItem());
    }
  private:
    QPoint pressPosition_;
    QPoint doubleClickPosition_;
};
int overviewCell(const QListWidget *list, const QListWidgetItem *item, QPoint position) {
    const auto hits = item->data(ArmyOverviewHits).value<QImage>();
    const auto local = position - list->visualItemRect(item).topLeft() - QPoint(6, 0);
    if (!hits.rect().contains(local) || !qAlpha(hits.pixel(local))) return -1;
    return qRed(hits.pixel(local)) - 1;
}
int overviewDropCell(const QListWidget *list, const QListWidgetItem *item, QPoint position) {
    const auto local=position-list->visualItemRect(item).topLeft()-QPoint(6,0);
    const auto cells=item->data(ArmyOverviewCells).toList();
    for(int cell=0;cell<cells.size();++cell)
        if(cells[cell].value<QPolygonF>().containsPoint(local,Qt::OddEvenFill))return cell;
    return -1;
}
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
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override {
        return {220,index.data(Qt::UserRole).toInt()<0?30:206};
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save();
        if(index.data(Qt::UserRole).toInt()<0) {
            p->fillRect(option.rect,QColor("#852c24")); p->setPen(QColor("#fff2d1"));
            p->drawText(option.rect.adjusted(8,0,-8,0),Qt::AlignVCenter,index.data().toString());
            p->restore(); return;
        }
        const auto image = index.data(ArmyOverviewImage).value<QImage>();
        p->drawImage(option.rect.topLeft()+QPoint(6,0), image);
        const auto list = qobject_cast<QListWidget *>(parent());
        if (list && list->property("armyDropUnit").toInt() == index.data(Qt::UserRole).toInt()) {
            const int cell = list->property("armyDropCell").toInt();
            const auto polygons = index.data(ArmyOverviewCells).toList();
            p->setPen(QPen(QColor("#fff291"), 3));
            p->setBrush(QColor(255, 242, 145, 60));
            if (cell >= 0 && cell < polygons.size())
                p->drawPolygon(polygons[cell].value<QPolygonF>().translated(option.rect.topLeft() + QPoint(6, 0)));
            else
                p->drawRoundedRect(option.rect.adjusted(2, 2, -2, -2), 4, 4);
        }
        const bool selected = option.state & QStyle::State_Selected;
        const QRect name(option.rect.left()+9,option.rect.bottom()-37,202,34);
        p->setPen(selected ? QColor("#fff291") : QColor("#fff2d1"));
        p->drawText(name.adjusted(5,1,-5,-1),Qt::AlignCenter|Qt::TextWordWrap,index.data().toString());
        if (selected) {
            p->setBrush(QColor("#fff291"));
            p->drawPolygon(QPolygon{option.rect.topLeft()+QPoint(14,42),option.rect.topLeft()+QPoint(26,48),option.rect.topLeft()+QPoint(14,54)});
        }
        p->restore();
    }
};
class ReserveDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        QStyledItemDelegate::paint(p,option,index);
        if(index.data(Qt::UserRole+10).toBool()) {
            p->save(); p->setBrush(Qt::NoBrush); p->setPen(QPen(QColor("#ffe89d"),3));
            p->drawRoundedRect(option.rect.adjusted(2,2,-2,-2),4,4); p->restore();
        }
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
    create_ = new QPushButton("Form Squad");
    create_->setObjectName("saveCreateUnit");
    creation->addWidget(create_); creation->addStretch();
    layout->addLayout(creation);
    creationNote_ = new QLabel;
    creationNote_->setWordWrap(true); layout->addWidget(creationNote_);
    connect(addCharacter_, &QPushButton::clicked, this, [this] {
        const int id=editor_->promptAddCharacter();
        if (id>=0) selectCharacter(id);
    });
    connect(recruitSquad_, &QPushButton::clicked, this, [this] {
        const int id=editor_->promptAddCharacter(true);
        if (id>=0) selectLeader(id);
    });
    connect(create_, &QPushButton::clicked, this, [this] {
        formingSquad_=!formingSquad_; refreshCreation();
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
    units_->setProperty("armyDropUnit", -1);
    units_->setItemDelegate(new ArmyUnitDelegate(units_));
    units_->setGridSize({220,206}); units_->setMinimumWidth(250);
    auto armyColumns=new QHBoxLayout;
    armyColumns->addWidget(units_,1);
    auto legionColumn=new QWidget;
    legionColumn->setObjectName("saveLegionColumn");
    auto legionLayout=new QVBoxLayout(legionColumn); legionLayout->setContentsMargins(0,0,0,0);
    auto legionTitle=new QLabel("LEGIONS"); legionLayout->addWidget(legionTitle);
    legions_=new ArmyList(floor); legions_->setObjectName("saveLegionList");
    legions_->setViewMode(QListView::ListMode); legions_->setWrapping(false);
    legions_->setItemDelegate(new ArmyUnitDelegate(legions_));
    legions_->setMinimumWidth(240); legions_->setMaximumWidth(265);
    legions_->setProperty("armyDropUnit",-1);
    legionLayout->addWidget(legions_,1); armyColumns->addWidget(legionColumn);
    rosterLayout->addLayout(armyColumns,1);
    auto reserveTitle = new QLabel("RESERVE CHARACTERS");
    reserveTitle->setWordWrap(true);
    reserveTitle->setStyleSheet("background:#ded6a6;border:1px solid #a4a05b;padding:4px;font-weight:bold;");
    rosterLayout->addWidget(reserveTitle);
    reserves_ = new ArmyList(floor);
    reserves_->setObjectName("saveArmyReserve");
    reserves_->setItemDelegate(new ReserveDelegate(reserves_));
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
    auto cardBody=makePage("Character");
    card_=new SaveCharacterCard(editor_); cardBody->addWidget(card_);
    auto formationBody=makePage("Squad");
    auto legionBody=makePage("Legion");
    auto itemsBody=makePage("Items");

    legion_ = new QLabel;
    legion_->setObjectName("saveUnitLegion");
    legionBody->addWidget(legion_);
    createLegion_ = new QPushButton("Create a legion from this command squad");
    createLegion_->setObjectName("saveCreateLegion");
    legionBody->addWidget(createLegion_);
    connect(createLegion_, &QPushButton::clicked, this, [this] {
        if ((shownUnit_ >= 0))
            editor_->createLegion(shownUnit_);
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
        if (legionTarget_->currentIndex() >= 0 && (shownUnit_ >= 0))
            editor_->joinLegion(legionTarget_->currentData().toInt(),
                                shownUnit_);
    });
    detachLegion_ = new QPushButton("Detach this squad from its legion");
    detachLegion_->setObjectName("saveDetachLegionUnit");
    legionBody->addWidget(detachLegion_);
    connect(detachLegion_, &QPushButton::clicked, this, [this] {
        if (shownLegion_ >= 0 && (shownUnit_ >= 0))
            editor_->detachLegionUnit(shownLegion_,
                                      shownUnit_);
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
    formationBody->addWidget(leader_);
    disband_ = new QPushButton("Disband this squad to reserve");
    disband_->setObjectName("saveDisbandUnit");
    formationBody->addWidget(disband_);
    connect(disband_, &QPushButton::clicked, this, [this] {
        if ((shownUnit_ >= 0))
            editor_->disbandUnit(shownUnit_);
    });
    connect(leader_, &QPushButton::clicked, this, [this] {
        if ((shownUnit_ >= 0))
            editor_->changeUnitLeader(shownUnit_,
                                      members_->currentRow());
    });
    auto help =
        new QLabel("Click a character to edit; drag to move or swap.");
    help->setWordWrap(true);
    formationBody->addWidget(help);
    board_ = new FormationBoard(editor_->project(), "saveUnitCell", true);
    board_->setPixelScale(3);
    board_->setExternalDragEnabled(true);
    board_->setAcceptDrops(true);
    board_->installEventFilter(this);
    for (auto list : {units_, legions_, reserves_}) {
        list->setAcceptDrops(true);
        list->viewport()->setAcceptDrops(true);
        list->viewport()->installEventFilter(this);
    }
    for (auto list : {units_, legions_, members_, reserves_})
        list->setDragEnabled(true);
    for (auto list : {units_,legions_})
        static_cast<ArmyMemberList *>(list)->dragMember = [this,list](QListWidgetItem *item) {
            if (!editor_->document()) return;
            const int cell=overviewCell(list,item,static_cast<ArmyMemberList *>(list)->dragPosition());
            for (const auto &unit:editor_->document()->units())
                if (unit.index==item->data(Qt::UserRole).toInt())
                    for (int member=0;member<unit.members.size();++member)
                        if (unit.members[member] && unit.cells[member]==cell) {
                            selectUnit(unit.index); members_->setCurrentRow(member);
                            startMemberDrag(unit.index,member,unit.members[member]); return;
                        }
        };
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
        connect(button, &QPushButton::clicked, this, [this,cell] {
            for (int member=0;member<ids_.size();++member)
                if (ids_[member] && cells_[member]==cell) {
                    members_->setCurrentRow(member); selectCharacter(ids_[member]); break;
                }
        });
    }
    connect(board_, &FormationBoard::cellDragStarted, this, [this](int cell) {
        const QSignalBlocker block(members_);
        for (int i=0;i<ids_.size();++i)
            if (ids_[i] && cells_[i]==cell) {members_->setCurrentRow(i);break;}
    });
    connect(board_, &FormationBoard::cellMoved, this, &SaveUnitEditor::moveCell);
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
    auto suppliesGroup = new QGroupBox("Carried items");
    auto suppliesLayout = new QGridLayout(suppliesGroup);
    suppliesLayout->setColumnStretch(1, 1);
    suppliesLayout->setColumnStretch(3, 1);
    capacity_ = new QLabel;
    capacity_->setObjectName("saveUnitCapacity");
    capacity_->setWordWrap(true);
    suppliesLayout->addWidget(capacity_, 0, 0, 1, 4);
    for (int i = 0; i < 10; ++i) {
        auto choice = new SaveItemPicker(editor_->project(),false,"Choose squad item");
        choice->setObjectName(QString("saveUnitSupply%1").arg(i));
        choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        choice->setMinimumContentsLength(12);
        supplies_ << choice;
        suppliesLayout->addWidget(new QLabel(QString::number(i + 1)), i / 2 + 1, (i % 2) * 2);
        suppliesLayout->addWidget(choice, i / 2 + 1, (i % 2) * 2 + 1);
        connect(choice, &QComboBox::currentIndexChanged, this, [this] {
            if (loading_ || !(shownUnit_ >= 0))
                return;
            QVector<int> values;
            for (const auto *box : supplies_)
                values << box->currentData().toInt();
            if (!editor_->editUnitSupplies(shownUnit_,
                                           values))
                refreshSupplies();
        });
    }
    itemsBody->addWidget(suppliesGroup);
    auto detailArea = new QScrollArea;
    detailArea->setObjectName("saveUnitDetailScroll");
    detailArea->setWidgetResizable(true);
    detailArea->setMinimumWidth(440);
    detailArea->setWidget(detail);
    split->addWidget(detailArea);
    split->setStretchFactor(0, 3); split->setStretchFactor(1, 2);
    split->setSizes({660,480});
    connect(reserves_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int id=item->data(Qt::UserRole).toInt();
        if (formingSquad_) {
            if (!editor_->document()->newUnitLeaders(editor_->project()->classData()).contains(id)) return;
            formingSquad_=false;
            if (editor_->createUnit(id,4)) selectLeader(id);
            refreshCreation();
        } else selectCharacter(id);
    });
    for (auto list : {units_,legions_}) {
        connect(list,&QListWidget::currentItemChanged,this,[this,list](QListWidgetItem *item) {
            if (loading_ || !item || item->data(Qt::UserRole).toInt()<0) return;
            auto other=list==units_ ? legions_ : units_;
            const QSignalBlocker block(other); other->setCurrentRow(-1);
            shownUnit_=item->data(Qt::UserRole).toInt(); showUnit();
            if(!ids_.isEmpty())card_->selectCharacter(ids_.first());
        });
        connect(list,&QListWidget::itemClicked,this,[this,list](QListWidgetItem *item) {
            if (!editor_->document() || item->data(Qt::UserRole).toInt()<0) return;
            const int cell=overviewCell(list,item,static_cast<ArmyMemberList *>(list)->dragPosition());
            selectUnit(item->data(Qt::UserRole).toInt());
            for (int member=0;member<ids_.size();++member)
                if (ids_[member] && cells_[member]==cell) {
                    members_->setCurrentRow(member); selectCharacter(ids_[member]); return;
                }
        });
    }
    connect(members_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (loading_) return;
        drawFormation();
        if (row>=0 && row<ids_.size()) selectCharacter(ids_[row]);
    });
    connect(unity_,&QSpinBox::valueChanged,this,[this]{commitFormation();});
    unity_->setKeyboardTracking(false);
    connect(tactic_,&QComboBox::activated,this,[this]{commitFormation();});
    connect(editor_, &SaveEditor::refreshed, this, [this] { refresh(); });
    refresh();
}
void SaveUnitEditor::startMemberDrag(int unit, int member, int id) {
    if (!editor_->document() || !editor_->document()->canEditUnits() ||
        formingSquad_ || id <= 0)
        return;
    auto drag = new QDrag(this);
    drag->setMimeData(new SaveArmyDrag(this, unit, member, id));
    const auto sprite = memberSprite(id);
    const auto scaled=sprite.scaled(64,64,Qt::KeepAspectRatio,Qt::FastTransformation);
    QPixmap preview(160,scaled.height()+26); preview.fill(Qt::transparent);
    QPainter painter(&preview);
    painter.drawImage(QPoint((preview.width()-scaled.width())/2,0),scaled);
    painter.setPen(QColor("#fff2d1"));
    painter.drawText(QRect(0,scaled.height(),160,26),Qt::AlignCenter,
        painter.fontMetrics().elidedText(memberName(id),Qt::ElideRight,156));
    painter.end();
    drag->setPixmap(preview); drag->setHotSpot({80,scaled.height()});
    QPixmap cursor(1,1);cursor.fill(Qt::transparent);
    for(auto action:{Qt::MoveAction,Qt::CopyAction,Qt::LinkAction,Qt::IgnoreAction})drag->setDragCursor(cursor,action);
    drag->exec(Qt::MoveAction);
    board_->setDropCell(-1);
    for(auto list:{units_,legions_}) {list->setProperty("armyDropUnit",-1);list->viewport()->update();}
    drag->deleteLater();
}
bool SaveUnitEditor::dropMember(const SaveArmyDrag *drag, QObject *target, QPoint position,
                                bool apply) {
    if (!drag || drag->owner != this || !editor_->document() ||
        !editor_->document()->canEditUnits() || formingSquad_)
        return false;
    const auto &doc = *editor_->document();
    const auto all = doc.units();
    if (drag->unit < 0) {
        if (!(drag->id==1000 && doc.reserveSoldiers()>0) && !doc.reserveMembers().contains(drag->id))
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
        } else if (target == units_->viewport() || target==legions_->viewport()) {
            auto list=target==units_->viewport()?units_:legions_;
            const auto item = list->itemAt(position);
            if (!item || item->data(Qt::UserRole).toInt()<0)
                return false;
            destination = item->data(Qt::UserRole).toInt();
            const int cell = overviewDropCell(list, item, position);
            if (cell >= 0) cells.append(cell);
            else for (int candidate = 0; candidate < 9; ++candidate) cells.append(candidate);
        } else {
            return false;
        }
        QString rejection;
        for (int cell : cells) {
            const auto to = std::find_if(all.begin(), all.end(),
                [&](const auto &unit) { return unit.index == destination; });
            if (to == all.end()) return false;
            int occupant = -1;
            for (int member = 0; member < to->members.size(); ++member)
                if (to->members[member] && to->cells[member] == cell) occupant = member;
            // A nameplate drop chooses a free cell; only an explicit member drop swaps.
            const bool swap = drag->unit >= 0 && occupant >= 0 && cells.size() == 1;
            auto formation = to->cells;
            try {
                if (drag->unit == destination) {
                    if (cells.size() != 1) return false;
                    if (occupant >= 0) formation[occupant] = formation[drag->member];
                    formation[drag->member] = cell;
                    doc.planUnitEdit(destination, formation, to->unity, to->tactic, editor_->project()->classData());
                } else if (swap)
                    doc.planUnitSwap(drag->unit, drag->member, destination, occupant, editor_->project()->classData());
                else if (drag->id==1000)
                    doc.planSoldierGroup(destination,cell,editor_->project()->classData());
                else if (drag->unit < 0)
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
            const bool moved = drag->unit == destination
                ? editor_->editUnit(destination, formation, to->unity, to->tactic)
                : swap ? editor_->swapUnitMembers(drag->unit, drag->member, destination, occupant)
                : drag->id==1000 ? editor_->addSoldierGroup(destination,cell)
                : drag->unit < 0
                ? editor_->addFromReserve(drag->id, destination, cell)
                : editor_->transferUnitMember(drag->unit, drag->member, destination, cell);
            if (moved) selectUnit(destination);
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
        for(auto list:{units_,legions_}) {list->setProperty("armyDropUnit",-1);list->viewport()->update();}
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
                            editor_->document()->canEditUnits() && !formingSquad_
                      : dropMember(drag, target, drop->position().toPoint(), event->type() == QEvent::Drop));
        if (valid) {
            drop->setDropAction(Qt::MoveAction);
            drop->accept();
        } else {
            drop->ignore();
        }
        board_->setDropCell(valid && target == board_ && event->type() == QEvent::DragMove
                                ? board_->cellAt(drop->position().toPoint()) : -1);
        for(auto list:{units_,legions_}) {
            int overview=-1,cell=-1;
            if(valid && target==list->viewport() && event->type()==QEvent::DragMove)
                if(const auto item=list->itemAt(drop->position().toPoint())) {
                    overview=item->data(Qt::UserRole).toInt(); cell=overviewDropCell(list,item,drop->position().toPoint());
                }
            list->setProperty("armyDropUnit",overview); list->setProperty("armyDropCell",cell); list->viewport()->update();
        }
        return true;
    }
    return QWidget::eventFilter(target, event);
}
void SaveUnitEditor::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // Include overview/reserve sprites when returning from class or artwork edits.
    refresh();
}
QString SaveUnitEditor::memberName(int id) const {
    if(id==1000)return "Soldiers";
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
    if(id==1000)id=100;
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
    for (const auto &unit:editor_->document()->units()) if (unit.members[0]==id) {selectUnit(unit.index);selectCharacter(id);return;}
}
void SaveUnitEditor::selectUnit(int index) {
    for (auto list:{units_,legions_}) for (int row=0;row<list->count();++row)
        if (list->item(row)->data(Qt::UserRole).toInt()==index) {
            const bool changed=shownUnit_!=index; shownUnit_=index;
            const QSignalBlocker a(units_),b(legions_);
            (list==units_ ? legions_ : units_)->setCurrentRow(-1);
            list->setCurrentRow(row); list->scrollToItem(list->item(row));
            if (changed || ids_.isEmpty()) {showUnit();if(!ids_.isEmpty())card_->selectCharacter(ids_.first());} return;
        }
}
void SaveUnitEditor::selectCharacter(int id) {
    if (loading_) return;
    card_->selectCharacter(id); detailTabs_->setCurrentIndex(0);
}
void SaveUnitEditor::commitFormation() {
    if (loading_ || shownUnit_<0) return;
    if (!editor_->editUnit(shownUnit_,cells_,unity_->value(),tactic_->currentData().toInt())) showUnit();
}
void SaveUnitEditor::moveCell(int from,int to) {
    if (loading_ || shownUnit_<0 || from<0 || to<0) return;
    int member=-1;
    for(int i=0;i<ids_.size();++i)if(ids_[i] && cells_[i]==from){member=i;break;}
    if (member<0 || member>=ids_.size() || !ids_[member]) return;
    auto next=cells_;
    for (int i=0;i<ids_.size();++i) if (ids_[i] && cells_[i]==to) next[i]=from;
    next[member]=to;
    if (!editor_->editUnit(shownUnit_,next,unity_->value(),tactic_->currentData().toInt())) showUnit();
}
void SaveUnitEditor::refresh() {
    const ViewPosition keepView(this);
    loading_=true;
    const int prior=shownUnit_,character=card_->selectedCharacter();
    const int reservePrior=reserves_->currentItem()?reserves_->currentItem()->data(Qt::UserRole).toInt():-1;
    units_->clear(); legions_->clear(); reserves_->clear();
    int soldiers=0,characters=0,count=0; quint32 funds=0;
    if (editor_->document()) {
        const auto &doc=*editor_->document(); const auto all=doc.units(); count=all.size();
        characters=doc.rows(SaveTable::Characters).size(); soldiers=doc.reserveSoldiers();
        funds=doc.value(SaveTable::Game,0,"goth").toUInt();
        QSet<int> grouped;
        for (const auto &legion:doc.legions()) for(int id:legion.units) if(id!=255)grouped.insert(id);
        FormationBoard thumbnail(editor_->project(),"armyThumbnail",true);
        auto append=[&](const SaveUnit &unit,QListWidget *list,bool commander=false) {
            thumbnail.clearUnits();
            for (int i=0;i<unit.members.size();++i) if (unit.members[i]) {
                const int member=unit.members[i];
                if(member>=100 && member<220)soldiers+=doc.soldiersInGroup(member);
                int classId=-1;
                try {classId=member>=100 && member<220?0:doc.value(SaveTable::Characters,member,"class").toInt();} catch(const Error &){}
                thumbnail.setUnit(unit.cells[i],classId,memberName(member));
            }
            auto item=new QListWidgetItem((commander?"Command · ":"Squad "+QString::number(unit.label)+" · ")+memberName(unit.members[0]),list);
            item->setData(Qt::UserRole,unit.index); item->setData(ArmyOverviewImage,thumbnail.overviewImage());
            item->setData(ArmyOverviewHits,thumbnail.overviewHitMap());
            QVariantList polygons;
            for(int cell=0;cell<9;++cell)polygons.append(QVariant::fromValue(thumbnail.overviewCellPolygon(cell)));
            item->setData(ArmyOverviewCells,polygons);
            item->setToolTip(item->text());
            if(unit.index==prior)list->setCurrentItem(item);
        };
        for(const auto &unit:all)if(!grouped.contains(unit.index))append(unit,units_);
        for(const auto &legion:doc.legions()) {
            auto heading=new QListWidgetItem(QString("Legion %1").arg(legion.label),legions_);
            heading->setData(Qt::UserRole,-1); heading->setFlags(Qt::ItemIsEnabled); heading->setSizeHint({220,30});
            for(int i=0;i<legion.units.size();++i)
                for(const auto &unit:all)if(unit.index==legion.units[i])append(unit,legions_,i==0);
        }
        for(int id:doc.reserveMembers()) {
            const auto image=memberSprite(id);
            const QIcon icon=image.isNull()?QIcon{}:QIcon(QPixmap::fromImage(image.scaled(image.size()*2,Qt::IgnoreAspectRatio,Qt::FastTransformation)));
            auto item=new QListWidgetItem(icon,memberName(id),reserves_);item->setData(Qt::UserRole,id);
            if(id==reservePrior)reserves_->setCurrentItem(item);
        }
        if(doc.reserveSoldiers()>0) {
            auto item=new QListWidgetItem(QIcon(QPixmap::fromImage(memberSprite(1000))),
                QString("Soldiers (%1)").arg(doc.reserveSoldiers()),reserves_);
            item->setData(Qt::UserRole,1000);item->setToolTip("Drag to an empty squad cell to form a Soldier group");
        }
        if(!std::any_of(all.cbegin(),all.cend(),[&](const auto &unit){return unit.index==prior;}))shownUnit_=all.isEmpty()?-1:all.first().index;
    } else shownUnit_=-1;
    legions_->parentWidget()->setVisible(legions_->count()>0);
    armyCounts_->setText(QString("SOLDIERS   %1       |       CHARACTERS   %2       |       UNITS   %3 / 30").arg(soldiers).arg(characters).arg(count));
    funds_->setText(QString("WAR FUNDS     %1 Goth").arg(QLocale().toString(funds)));
    loading_=false; showUnit(); selectUnit(shownUnit_);
    if(character>0)card_->selectCharacter(character);
}
void SaveUnitEditor::showUnit() {
    if (loading_)
        return;
    loading_ = true;
    const int selectedCharacter=card_->selectedCharacter();
    members_->clear();
    ids_.clear();
    cells_.clear();
    tactic_->clear();
    for (const auto &label : {"Autonomous", "Attack Strongest", "Attack Leader", "Attack Weakest"})
        tactic_->addItem(label, tactic_->count());
    if (editor_->document() && (shownUnit_ >= 0)) {
        const int index = shownUnit_;
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
            members_->setCurrentRow(qMax(0,ids_.indexOf(selectedCharacter)));
            break;
        }
    }
    const bool editable = editor_->document() && editor_->document()->canEditUnits();
    note_->setText(editable ? "Drag characters between squads and Reserve. Click a character to edit."
                            : editor_->document() ? "Viewing a runtime/suspend save. Use a regular battery-save slot to create or reorganize squads."
                                                  : "Open a save to view and manage your army.");
    unity_->setEnabled(editable && !ids_.isEmpty());
    tactic_->setEnabled(unity_->isEnabled());
    loading_ = false;
    drawFormation();
    if (!ids_.isEmpty() && card_->selectedCharacter()<=0) card_->selectCharacter(ids_.first());
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
        grid_[cell]->setEnabled(unity_->isEnabled());
    }
    board_->finishUpdate();
    refreshControls();
}
void SaveUnitEditor::refreshControls() {
    if (loading_) return;
    refreshSupplies(); refreshCreation();
    shownLegion_ = -1;
    bool follower = false;
    legion_->setText("Independent squad");
    if (editor_->document() && (shownUnit_ >= 0))
        for (const auto &legion : editor_->document()->legions())
            if (legion.units.contains(shownUnit_)) {
                shownLegion_ = legion.index;
                follower = legion.units[0] != shownUnit_;
                const int count = int(std::count_if(legion.units.begin(), legion.units.end(),
                                                    [](int id) { return id != 255; }));
                legion_->setText(QString("Legion %1 · %2 squads").arg(legion.label).arg(count));
                break;
            }
    createLegion_->setEnabled(editor_->document() && (shownUnit_ >= 0) &&
                              editor_->document()->legions().size() < 6 &&
                              editor_->document()
                                  ->newLegionCommanders(editor_->project()->classData())
                                  .contains(shownUnit_));
    createLegion_->setToolTip(
        "Starts a legion with this independent command squad. Add followers with Join "
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
    const bool canJoin = (shownUnit_ >= 0) && legionTarget_->count();
    legionTarget_->setEnabled(canJoin);
    joinLegion_->setEnabled(canJoin);
    joinLegion_->setToolTip(
        "Assigns this independent squad to the first free follower position. "
                  "Each follower requires a Soldier group in the command squad.");
    detachLegion_->setEnabled(follower && editor_->document()->canEditUnits());
    detachLegion_->setToolTip(
        "Keeps this squad and its contents; the remaining legion retains its commander. "
                  "Use Disband legion to release the command squad.");
    disbandLegion_->setEnabled(shownLegion_ >= 0 && editor_->document()->canEditUnits());
    disbandLegion_->setToolTip("Makes all squads independent, retaining their leaders, "
                                     "formations, Unity and supplies; Undo restores the legion");
    disband_->setEnabled(editor_->document() && editor_->document()->canEditUnits() &&
                         (shownUnit_ >= 0));
    disband_->setToolTip(
        "Returns ordinary members and Soldiers to reserve and releases carried items "
                  "without changing total ownership; Undo restores the whole squad");
    const int member=members_->currentRow();
    leader_->setEnabled(editor_->document() && editor_->document()->canEditUnits() &&
        member>0 && member<ids_.size() && ids_[member]>0 && ids_[member]<100);
}
void SaveUnitEditor::refreshCreation() {
    const bool editable=editor_->document() && editor_->document()->canEditUnits();
    const bool slot=editable && editor_->document()->freeCharacterSlot()>=0;
    const bool squadSlot=editable && editor_->document()->units().size()<30;
    const auto leaders=editable ? editor_->document()->newUnitLeaders(editor_->project()->classData()) : QVector<int>{};
    if (!squadSlot || leaders.isEmpty()) formingSquad_=false;
    addCharacter_->setEnabled(slot); recruitSquad_->setEnabled(slot && squadSlot);
    create_->setEnabled(squadSlot && !leaders.isEmpty());
    create_->setText(formingSquad_ ? "Cancel Form Squad" : "Form Squad");
    creationNote_->setText(formingSquad_ ? "Choose a highlighted reserve leader."
        : !editable && editor_->document() ? "Forming squads requires a regular battery-save slot." : QString{});
    creationNote_->setVisible(!creationNote_->text().isEmpty());
    reserves_->setProperty("formingSquad",formingSquad_);
    for (int row=0;row<reserves_->count();++row) {
        auto item=reserves_->item(row); const bool eligible=leaders.contains(item->data(Qt::UserRole).toInt());
        item->setData(Qt::UserRole+10,formingSquad_ && eligible);
        item->setFlags((formingSquad_ && !eligible) ? Qt::NoItemFlags : Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsDragEnabled);
    }
    reserves_->viewport()->update();
}
void SaveUnitEditor::refreshSupplies() {
    QVector<int> values(10, 0);
    bool editable = false;
    int capacity = 0;
    int selected = -1;
    QString reason;
    if (editor_->document() && (shownUnit_ >= 0)) {
        const int index = shownUnit_;
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
        box->setEnabled(editable);
        box->setToolTip(reason.isEmpty() ? "Choose an unassigned copy from owned inventory; "
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
