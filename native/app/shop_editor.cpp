#include "app/shop_editor.h"
#include <QComboBox>
#include <QCompleter>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
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
ShopEditor::ShopEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                       QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("shopEditor");
    auto layout = new QVBoxLayout(this);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Find a shop by town, item name, or shop number…");
    search_->setClearButtonEnabled(true);
    search_->setObjectName("shopSearch");
    layout->addWidget(search_);
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
    for (int id = 1; id < Shops::count; ++id) {
        auto row = new QListWidgetItem(shops_);
        row->setData(Qt::UserRole, id);
        copyFrom_->addItem(
            QString("Shop %1 — %2").arg(id).arg(project_->shops()->locations(id).join(", ")), id);
    }
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
    inventory.choice = new QComboBox;
    inventory.choice->setObjectName(key + "Choice");
    inventory.choice->setEditable(true);
    inventory.choice->setInsertPolicy(QComboBox::NoInsert);
    inventory.choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    inventory.choice->setMinimumContentsLength(16);
    inventory.choice->completer()->setFilterMode(Qt::MatchContains);
    inventory.choice->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    layout->addWidget(inventory.choice);
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
    for (int n = 0; n < shops_->count(); ++n) {
        const auto id = n + 1;
        const auto &stock = project_->shops()->stock(id);
        const auto label = QString("Shop %1%2\n%3")
                               .arg(id)
                               .arg(stock == project_->shops()->originalStock(id) ? "" : " •")
                               .arg(project_->shops()->locations(id).join(", "));
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
    heading_->setText(
        QString("Shop %1 · %2").arg(id).arg(project_->shops()->locations(id).join(", ")));
    for (bool equipment : {true, false}) {
        auto &i = equipment ? equipment_ : consumables_;
        const auto &stock = project_->shops()->stock(id);
        const auto &ids = equipment ? stock.equipment : stock.consumables;
        const auto capacity = equipment ? 50 : 30, maximum = equipment ? 277 : 44;
        const auto previous =
            i.list->currentItem() ? i.list->currentItem()->data(Qt::UserRole).toInt() : -1;
        const auto choice = i.choice->currentData().toInt();
        i.list->clear();
        i.choice->clear();
        for (auto item : ids) {
            auto row = new QListWidgetItem(
                QString("%1  ·  %2").arg(itemName(equipment, item)).arg(item), i.list);
            row->setData(Qt::UserRole, item);
            if (item == previous)
                i.list->setCurrentItem(row);
        }
        for (int item = 1; item <= maximum; ++item)
            if (!ids.contains(quint16(item)))
                i.choice->addItem(QString("%1  ·  %2").arg(itemName(equipment, item)).arg(item),
                                  item);
        if (i.choice->findData(choice) >= 0)
            i.choice->setCurrentIndex(i.choice->findData(choice));
        i.count->setText(QString("%1 of %2 entries").arg(ids.size()).arg(capacity));
        i.add->setEnabled(ids.size() < capacity && i.choice->count());
        i.choice->setEnabled(i.add->isEnabled());
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
void ShopEditor::change(bool equipment, int operation) {
    const auto id = selected();
    if (id < 1)
        return;
    auto &i = equipment ? equipment_ : consumables_;
    auto stock = project_->shops()->stock(id);
    auto &ids = equipment ? stock.equipment : stock.consumables;
    const auto row = i.list->currentRow();
    if (operation == 0) {
        const auto choice = i.choice->findText(i.choice->currentText());
        if (choice < 0) {
            notice_->setText("Choose an item from the list before adding it.");
            return;
        }
        ids << quint16(i.choice->itemData(choice).toInt());
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
