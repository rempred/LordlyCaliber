#include "app/view_position.h"
#include "app/shop_editor.h"
#include "app/item_picker.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
namespace lordly {
class ShopCommand final : public QUndoCommand {
  public:
    ShopCommand(Project *project, QString label, QJsonObject before, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override {
        project_->setShops(before_);
    }
    void redo() override {
        project_->setShops(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
namespace {
class ShopFacilityCommand final : public QUndoCommand {
  public:
    ShopFacilityCommand(Project *project, Project after, const QString &label)
        : QUndoCommand(label), project_(project), before_(*project), after_(std::move(after)) {}
    void undo() override {
        *project_ = before_;
    }
    void redo() override {
        *project_ = after_;
    }

  private:
    Project *project_;
    Project before_, after_;
};
void setShop(Project &project, int townId, int shopId) {
    project.loadWorld();
    project.loadShops();
    if (shopId < 0 || shopId >= project.shops()->size())
        throw Error("Choose an existing shop");
    auto world = *project.world();
    auto town = world.town(townId);
    if (shopId && u8(town, 24) == 255)
        throw Error(
            "Mission objectives cannot open shops. Clear Mission objective in Sites first.");
    put8(town, 27, quint8(shopId));
    put8(town, 25, quint8((u8(town, 25) & ~1) | (shopId ? 1 : 0)));
    world.setTown(townId, town);
    project.setWorld(world.json());
}
} // namespace
int addShop(Project *project, QUndoStack *undo, int townId) {
    auto next = *project;
    next.loadShops();
    auto shops = *next.shops();
    const int id = shops.addShop();
    next.setShops(shops.json());
    if (townId > 0)
        setShop(next, townId, id);
    undo->push(new ShopFacilityCommand(project, std::move(next), "Add shop"));
    return id;
}
void assignShop(Project *project, QUndoStack *undo, int townId, int shopId) {
    auto next = *project;
    setShop(next, townId, shopId);
    if (next.manifest() != project->manifest())
        undo->push(
            new ShopFacilityCommand(project, std::move(next),
                                    shopId ? "Assign stronghold shop" : "Remove stronghold shop"));
}
ShopEditor::ShopEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                       QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("shopEditor");
    project_->loadWorld();
    project_->loadScenarios();
    project_->loadIconArtwork();
    auto layout = new QVBoxLayout(this);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Find a shop by town, item name, or shop number…");
    search_->setClearButtonEnabled(true);
    search_->setObjectName("shopSearch");
    layout->addWidget(search_);
    auto add = new QPushButton("Add shop");
    add->setObjectName("shopAdd");
    layout->addWidget(add, 0, Qt::AlignLeft);
    connect(add, &QPushButton::clicked, this, [this] {
        try {
            const int id = addShop(project_, undo_);
            search_->clear();
            shops_->setCurrentRow(id - 1);
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    auto split = new QSplitter;
    layout->addWidget(split);
    shops_ = new QListWidget;
    shops_->setObjectName("shopList");
    shops_->setMinimumWidth(220);
    split->addWidget(shops_);
    details_ = new QWidget;
    auto detail = new QVBoxLayout(details_);
    heading_ = new QLabel;
    heading_->setWordWrap(true);
    auto font = heading_->font();
    font.setPointSize(font.pointSize() + 3);
    font.setBold(true);
    heading_->setFont(font);
    detail->addWidget(heading_);
    auto assignment = new QHBoxLayout;
    town_ = new QComboBox;
    town_->setObjectName("shopStronghold");
    town_->setEditable(true);
    town_->setInsertPolicy(QComboBox::NoInsert);
    town_->completer()->setFilterMode(Qt::MatchContains);
    town_->setMinimumContentsLength(30);
    town_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    town_->setToolTip("Every stronghold, grouped by scenario. A shared stronghold's shop changes "
                      "in all scenarios that use it.");
    assignment->addWidget(town_, 1);
    auto attach = new QPushButton("Assign to stronghold");
    attach->setObjectName("shopAssign");
    auto detach = new QPushButton("Remove stronghold's shop");
    detach->setObjectName("shopDetach");
    assignment->addWidget(attach);
    assignment->addWidget(detach);
    detail->addLayout(assignment);
    for (const auto button : {attach, detach})
        connect(button, &QPushButton::clicked, this, [this, button, attach] {
            try {
                if (town_->currentIndex() < 0 ||
                    town_->currentText() != town_->itemText(town_->currentIndex()))
                    throw Error("Choose a stronghold from the list");
                assignShop(project_, undo_, town_->currentData().toInt(),
                           button == attach ? selected() : 0);
            } catch (const Error &e) {
                notice_->setText(QString::fromUtf8(e.what()));
            }
        });
    auto inventories = new QHBoxLayout;
    inventories->addWidget(makeInventory(true));
    inventories->addWidget(makeInventory(false));
    detail->addLayout(inventories, 1);
    auto actions = new QHBoxLayout;
    copyFrom_ = new QComboBox;
    copyFrom_->setObjectName("shopCopySource");
    auto copy = new QPushButton("Copy stock from…");
    copy->setObjectName("shopCopy");
    actions->addWidget(copy);
    actions->addWidget(copyFrom_, 1);
    auto reset = new QPushButton("Restore loaded stock");
    reset->setObjectName("shopRestore");
    actions->addWidget(reset);
    detail->addLayout(actions);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details_);
    split->setStretchFactor(1, 1);
    split->setSizes({300, 950});
    connect(shops_, &QListWidget::currentRowChanged, this, [this] { refresh(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(copy, &QPushButton::clicked, this, [this] {
        if (selected() > 0)
            edit("Copy shop stock", project_->shops()->stock(copyFrom_->currentData().toInt()));
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        if (selected() > 0)
            edit("Restore shop stock", project_->shops()->originalStock(selected()));
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    shops_->setCurrentRow(0);
    refresh();
}
QWidget *ShopEditor::makeInventory(bool equipment) {
    auto &inventory = equipment ? equipment_ : consumables_;
    const auto key = equipment ? QString("shopEquipment") : QString("shopConsumables");
    auto group = new QGroupBox(equipment ? "Equipment" : "Consumables");
    auto layout = new QVBoxLayout(group);
    inventory.count = new QLabel;
    layout->addWidget(inventory.count);
    inventory.list = new QListWidget;
    inventory.list->setObjectName(key + "List");
    layout->addWidget(inventory.list, 1);
    auto buttons = new QHBoxLayout;
    const QStringList labels = {"Add", "Remove", "Up", "Down"};
    QPushButton **buttonSlots[] = {&inventory.add, &inventory.remove, &inventory.up,
                                   &inventory.down};
    for (int action = 0; action < 4; ++action) {
        auto button = new QPushButton(labels[action]);
        button->setObjectName(key + labels[action]);
        *buttonSlots[action] = button;
        buttons->addWidget(button);
        connect(button, &QPushButton::clicked, this,
                [this, equipment, action] { change(equipment, action); });
    }
    layout->addLayout(buttons);
    connect(inventory.list, &QListWidget::currentRowChanged, this, [this, equipment] {
        auto &i = equipment ? equipment_ : consumables_;
        const auto row = i.list->currentRow();
        i.remove->setEnabled(row >= 0);
        i.up->setEnabled(row > 0);
        i.down->setEnabled(row >= 0 && row + 1 < i.list->count());
    });
    return group;
}
QString ShopEditor::itemName(bool equipment, int id) const {
    if (equipment)
        return project_->record(Table::Items, id).name;
    return project_->catalog()["CONSUMABLE_NAMES"].toObject()[QString::number(id)].toString(
        QString("Consumable %1").arg(id));
}
int ShopEditor::selected() const {
    return shops_->currentItem() ? shops_->currentItem()->data(Qt::UserRole).toInt() : -1;
}
void ShopEditor::filter() {
    const auto query = search_->text().trimmed();
    for (int n = 0; n < shops_->count(); ++n) {
        auto row = shops_->item(n);
        row->setHidden(
            !row->data(Qt::UserRole + 1).toString().contains(query, Qt::CaseInsensitive));
    }
}
void ShopEditor::refresh() {
    const ViewPosition keepView(this);
    const int keep = selected(), copy = copyFrom_->currentData().toInt();
    QSignalBlocker block(shops_), copyBlock(copyFrom_);
    const int scroll = shops_->verticalScrollBar()->value();
    while (shops_->count() >= project_->shops()->size())
        delete shops_->takeItem(shops_->count() - 1);
    while (shops_->count() + 1 < project_->shops()->size()) {
        auto row = new QListWidgetItem(shops_);
        row->setData(Qt::UserRole, shops_->count());
    }
    copyFrom_->clear();
    for (int id = 1; id < project_->shops()->size(); ++id)
        copyFrom_->addItem(QString("Shop %1 — %2").arg(id).arg(locations(id).join(", ")), id);
    shops_->setCurrentRow(qBound(0, keep - 1, shops_->count() - 1));
    copyFrom_->setCurrentIndex(qMax(0, copyFrom_->findData(copy)));
    findChild<QPushButton *>("shopAdd")->setEnabled(project_->shops()->size() < 256);
    for (int n = 0; n < shops_->count(); ++n) {
        const auto id = n + 1;
        const auto &stock = project_->shops()->stock(id);
        const auto label = QString("Shop %1%2\n%3")
                               .arg(id)
                               .arg(stock == project_->shops()->originalStock(id) ? "" : " •")
                               .arg(locations(id).join(", "));
        auto searchable = label;
        for (auto item : stock.equipment)
            searchable += " " + itemName(true, item);
        for (auto item : stock.consumables)
            searchable += " " + itemName(false, item);
        shops_->item(n)->setText(label);
        shops_->item(n)->setData(Qt::UserRole + 1, searchable);
    }
    filter();
    const auto id = selected();
    details_->setEnabled(id > 0);
    if (id < 1)
        return;
    heading_->setText(QString("Shop %1 · %2").arg(id).arg(locations(id).join(", ")));
    const QSignalBlocker townBlock(town_);
    const int previousTown = town_->currentData().toInt();
    town_->clear();
    town_->addItem("Choose a stronghold…", -1);
    QSet<int> listed;
    for (const auto &scenario : project_->scenarios()->entries()) {
        const auto &sites = project_->world()->scenario(scenario.key).sites;
        for (int row = 0; row < project_->world()->siteCount(scenario.key); ++row) {
            const int townId = u16(sites, 1 + row * 4);
            listed.insert(townId);
            town_->addItem(QString("%1 · %2 / %3 [%4]")
                               .arg(scenario.key, 2, 10, QChar('0'))
                               .arg(scenario.name, project_->world()->townName(townId))
                               .arg(townId),
                           townId);
        }
    }
    for (int townId = 1; townId <= ScenarioWorld::townCount; ++townId)
        if (!listed.contains(townId))
            town_->addItem(QString("Other strongholds / %1 [%2]")
                               .arg(project_->world()->townName(townId))
                               .arg(townId),
                           townId);
    int assigned = -1;
    for (int at = 1; at < town_->count(); ++at) {
        const int townId = town_->itemData(at).toInt();
        const auto town = project_->world()->town(townId);
        if (u8(town, 27) == id && (u8(town, 25) & 1) && u8(town, 24) != 255) {
            if (assigned < 0 || townId == previousTown)
                assigned = at;
        }
    }
    town_->setCurrentIndex(assigned < 0 ? 0 : assigned);
    shops_->doItemsLayout();
    shops_->verticalScrollBar()->setValue(scroll);
    for (bool equipment : {true, false}) {
        auto &i = equipment ? equipment_ : consumables_;
        const auto &stock = project_->shops()->stock(id);
        const auto &ids = equipment ? stock.equipment : stock.consumables;
        const auto capacity = equipment ? 50 : 30;
        const auto previous =
            i.list->currentItem() ? i.list->currentItem()->data(Qt::UserRole).toInt() : -1;
        i.list->clear();
        for (auto item : ids) {
            auto row = new QListWidgetItem(
                QString("%1  ·  %2").arg(itemName(equipment, item)).arg(item), i.list);
            row->setData(Qt::UserRole, item);
            row->setIcon(
                QPixmap::fromImage(project_->iconArtwork()->image(item | (equipment ? 0 : 0x200))));
            if (item == previous)
                i.list->setCurrentItem(row);
        }
        i.count->setText(QString("%1 of %2 entries").arg(ids.size()).arg(capacity));
        i.add->setEnabled(ids.size() < capacity);
        if (i.list->currentRow() < 0 && i.list->count())
            i.list->setCurrentRow(0);
        const auto row = i.list->currentRow();
        i.remove->setEnabled(row >= 0);
        i.up->setEnabled(row > 0);
        i.down->setEnabled(row >= 0 && row + 1 < i.list->count());
    }
    notice_->clear();
}
void ShopEditor::edit(const QString &label, const ShopStock &stock) {
    const auto id = selected();
    if (id < 1)
        return;
    try {
        auto next = *project_->shops();
        next.setStock(id, stock);
        if (next.json() != project_->shops()->json())
            undo_->push(new ShopCommand(project_, label, project_->shops()->json(), next.json()));
    } catch (const std::exception &error) {
        notice_->setText(QString::fromUtf8(error.what()));
    }
}
QStringList ShopEditor::locations(int id) const {
    QStringList names;
    for (int townId = 1; townId <= ScenarioWorld::townCount; ++townId) {
        const auto town = project_->world()->town(townId);
        if (u8(town, 27) == id && (u8(town, 25) & 1) && u8(town, 24) != 255)
            names << project_->world()->townName(townId);
    }
    return names.isEmpty() ? QStringList{"Unassigned"} : names;
}
QVector<quint16> ShopEditor::chooseItems(bool equipment) {
    const auto &stock = project_->shops()->stock(selected());
    const auto &existing = equipment ? stock.equipment : stock.consumables;
    return chooseRomItems(project_, this, equipment, existing, (equipment ? 50 : 30) - existing.size());
}
void ShopEditor::change(bool equipment, int operation) {
    const auto id = selected();
    if (id < 1)
        return;
    auto &i = equipment ? equipment_ : consumables_;
    auto stock = project_->shops()->stock(id);
    auto &ids = equipment ? stock.equipment : stock.consumables;
    const auto row = i.list->currentRow();
    if (operation == 0) {
        const auto selectedItems = chooseItems(equipment);
        if (selectedItems.isEmpty())
            return;
        ids += selectedItems;
    } else if (row < 0 || row >= ids.size())
        return;
    else if (operation == 1)
        ids.removeAt(row);
    else {
        const auto other = row + (operation == 2 ? -1 : 1);
        if (other < 0 || other >= ids.size())
            return;
        ids.swapItemsAt(row, other);
    }
    edit(operation == 0   ? "Add shop item"
         : operation == 1 ? "Remove shop item"
                          : "Reorder shop stock",
         stock);
    if (operation == 0)
        i.list->setCurrentRow(i.list->count() - 1);
}
} // namespace lordly
