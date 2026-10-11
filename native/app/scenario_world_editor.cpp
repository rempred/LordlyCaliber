#include "app/scenario_world_editor.h"
#include "app/shop_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>

namespace lordly {
namespace {
class WorldCommand final : public QUndoCommand {
  public:
    WorldCommand(Project *p, const QString &label, QJsonObject before, QJsonObject after)
        : QUndoCommand(label), p_(p), before_(std::move(before)), after_(std::move(after)) {}
    void undo() override {
        p_->setWorld(before_);
    }
    void redo() override {
        p_->setWorld(after_);
    }

  private:
    Project *p_;
    QJsonObject before_, after_;
};
QSpinBox *spin(const QString &name, int maximum) {
    auto box = new QSpinBox;
    box->setObjectName(name);
    box->setRange(0, maximum);
    return box;
}
void select(QComboBox *box, int value) {
    int at = box->findData(value);
    if (at < 0) {
        box->addItem(QString("Loaded value %1").arg(value), value);
        at = box->count() - 1;
    }
    box->setCurrentIndex(at);
}
} // namespace
ScenarioWorldEditor::ScenarioWorldEditor(Project *project, QUndoStack *undo,
                                         std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    project_->loadWorld();
    project_->loadShops();
    setObjectName("mapWorldInspector");
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    siteFields_ = new QWidget;
    siteFields_->setObjectName("worldSiteFields");
    layout->addWidget(siteFields_);
    auto siteLayout = new QVBoxLayout(siteFields_);
    siteLayout->setContentsMargins(0, 0, 0, 0);
    siteHelp_ = new QLabel;
    siteHelp_->setWordWrap(true);
    siteLayout->addWidget(siteHelp_);
    auto form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    siteLayout->addLayout(form);
    name_ = new QLineEdit;
    name_->setObjectName("worldSiteName");
    name_->setMaxLength(20);
    form->addRow("Site name", name_);
    population_ = spin("worldPopulation", 65535);
    form->addRow("Population", population_);
    morale_ = spin("worldMorale", 255);
    form->addRow("Morale (0–127)", morale_);
    objective_ = new QCheckBox("Mission objective");
    objective_->setObjectName("worldObjective");
    objective_->setToolTip("Store the special objective-site value instead of morale. This marks "
                           "an objective site and prevents its shop from opening; it does not "
                           "create or rewrite the mission's victory events.");
    form->addRow(objective_);
    type_ = new QComboBox;
    type_->setObjectName("worldSiteType");
    type_->addItem("Town", 9);
    type_->addItem("Fort", 0x29);
    type_->addItem("Boss fort", 0x49);
    type_->addItem("Castle", 0x89);
    type_->setToolTip(
        "The game's stronghold category: town, fort, boss fort or castle. This changes site "
        "metadata; it does not change the map terrain or create a victory event.");
    form->addRow("Stronghold type", type_);
    auto facilityRow = new QGridLayout;
    for (const auto &label : {"Shop", "Witch’s den", "Treasure", "Mine"}) {
        auto check = new QCheckBox(label);
        check->setObjectName(QString("worldFacility%1").arg(facilities_.size()));
        facilities_ << check;
        if (facilities_.size() == 2)
            check->setToolTip(
                "Check to add a witch’s den, or clear to remove it. "
                "Apply site changes saves this facility for every scenario using the stronghold.");
        if (facilities_.size() > 2) {
            check->setText(facilities_.size() == 3 ? "Treasure flag"
                                                   : "Mine flag");
            check->setToolTip(
                "Original site capability flag. Its complete gameplay effect is not verified. This "
                "does not add a hidden treasure; use Add treasure on the Map tab for that.");
        }
        const int i = facilities_.size() - 1;
        facilityRow->addWidget(check, i / 2, i % 2);
    }
    form->addRow("Facilities", facilityRow);
    auto facilityHelp = new QLabel(
        "A town can show Shop or Witch’s den, not both. Apply site changes, export the ROM, "
        "then enter the mission from the world map. Resuming a suspend save keeps its old facilities.");
    facilityHelp->setWordWrap(true);
    form->addRow(facilityHelp);
    shop_ = spin("worldShop", project_->shops()->size() - 1);
    shop_->setSpecialValueText("None");
    form->addRow("Shop ID", shop_);
    auto shops = new QVBoxLayout;
    auto createShop = new QPushButton("Create new shop here");
    createShop->setObjectName("worldShopAdd");
    auto removeShop = new QPushButton("Remove shop from site");
    removeShop->setObjectName("worldShopRemove");
    shops->addWidget(createShop);
    shops->addWidget(removeShop);
    form->addRow(shops);
    connect(createShop, &QPushButton::clicked, this, [this] {
        if (siteId() < 0)
            return;
        try {
            addShop(project_, undo_, siteId());
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(removeShop, &QPushButton::clicked, this, [this] {
        if (siteId() < 0)
            return;
        try {
            assignShop(project_, undo_, siteId(), 0);
        } catch (const Error &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    allegiance_ = new QComboBox;
    allegiance_->setObjectName("worldAllegiance");
    allegiance_->addItem("Keep loaded value", -1);
    allegiance_->addItem("Neutral", 0);
    allegiance_->addItem("Allied", 1);
    allegiance_->addItem("Enemy", 2);
    form->addRow("Initial allegiance", allegiance_);
    auto applySite = new QPushButton("Apply site changes");
    applySite->setObjectName("worldSiteApply");
    siteLayout->addWidget(applySite);
    auto restoreSite = new QPushButton("Restore loaded site metadata");
    restoreSite->setObjectName("worldSiteRestore");
    siteLayout->addWidget(restoreSite);
    siteLayout->addStretch();
    treasureFields_ = new QWidget;
    treasureFields_->setObjectName("worldTreasureFields");
    layout->addWidget(treasureFields_);
    auto treasureLayout = new QVBoxLayout(treasureFields_);
    treasureLayout->setContentsMargins(0, 0, 0, 0);
    auto help = new QLabel(
        "Coordinates use the game's 0–255 map scale. Collection flags are shared across scenarios; "
        "editing an existing treasure retains its flag. New treasures receive an unused flag.");
    help->setWordWrap(true);
    treasureLayout->addWidget(help);
    flag_ = new QLabel;
    treasureLayout->addWidget(flag_);
    auto treasureForm = new QFormLayout;
    treasureForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    treasureLayout->addLayout(treasureForm);
    x_ = spin("worldTreasureX", 255);
    z_ = spin("worldTreasureZ", 255);
    treasureForm->addRow("X", x_);
    treasureForm->addRow("Z", z_);
    table_ = new QComboBox;
    table_->setObjectName("worldTreasureTable");
    table_->addItem("Equipment", 1);
    table_->addItem("Consumable", 2);
    item_ = new QComboBox;
    item_->setObjectName("worldTreasureItem");
    item_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    item_->setMinimumContentsLength(16);
    treasureForm->addRow("Item table", table_);
    treasureForm->addRow("Item", item_);
    auto applyTreasure = new QPushButton("Apply treasure changes");
    applyTreasure->setObjectName("worldTreasureApply");
    treasureLayout->addWidget(applyTreasure);
    auto remove = new QPushButton("Remove treasure");
    remove->setObjectName("worldTreasureRemove");
    treasureLayout->addWidget(remove);
    treasureLayout->addStretch();
    auto restore = new QPushButton("Restore scenario treasure and allegiance");
    restore->setObjectName("worldScenarioRestore");
    restore->setToolTip("Restore all loaded treasure and initial site allegiances in this scenario.");
    layout->addWidget(restore);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    notice_->setObjectName("worldNotice");
    layout->addWidget(notice_);
    for (int i = 0; i < 2; ++i)
        connect(facilities_[i], &QCheckBox::toggled, this, [this, i](bool on) {
            if (loading_) return;
            if (on) facilities_[1 - i]->setChecked(false);
            siteHelp_->setText("Unapplied facility changes — press Apply site changes, then export the ROM.");
        });
    connect(table_, &QComboBox::currentIndexChanged, this, [this] {
        if (!loading_)
            fillItems();
    });
    connect(objective_, &QCheckBox::toggled, this,
            [this](bool value) { morale_->setEnabled(!value); });
    connect(applySite, &QPushButton::clicked, this, [this] {
        if (siteId() < 0)
            return;
        change("Edit scenario site", [this](ScenarioWorld &next) {
            const int id = siteId();
            auto record = next.town(id);
            if (name_->text() != next.townName(id)) {
                const auto bytes = name_->text().toLatin1();
                if (QString::fromLatin1(bytes) != name_->text())
                    throw Error("Site names require printable ASCII");
                record.replace(1, 20, bytes + QByteArray(20 - bytes.size(), 0));
            }
            put16(record, 22, quint16(population_->value()));
            put8(record, 24, quint8(objective_->isChecked() ? 255 : morale_->value()));
            int caps = u8(record, 25) & 0xf0;
            for (int i = 0; i < 4; ++i)
                if (facilities_[i]->isChecked())
                    caps |= 1 << i;
            put8(record, 25, quint8(caps));
            if ((caps & 1) && !shop_->value())
                throw Error("This shop needs an inventory. Choose a Shop ID or use Create new shop here.");
            put8(record, 26, quint8(type_->currentData().toInt()));
            put8(record, 27, quint8(shop_->value()));
            next.setTown(id, record);
            if (allegiance_->currentData().toInt() >= 0)
                next.setSiteAllegiance(key_, siteRow_,
                                       allegiance_->currentData().toInt());
        });
    });
    connect(restoreSite, &QPushButton::clicked, this, [this] {
        if (siteId() >= 0)
            change("Restore site metadata", [this](ScenarioWorld &next) {
                next.restoreTown(siteId());
            });
    });
    connect(applyTreasure, &QPushButton::clicked, this, [this] {
        if (treasureRow_ < 0)
            return;
        change("Edit scenario treasure", [this](ScenarioWorld &next) {
            auto record = ScenarioWorld::treasureRecord(next.scenario(key_).treasures,
                                                        treasureRow_);
            put8(record, 1, quint8(x_->value()));
            put8(record, 2, quint8(z_->value()));
            put8(record, 3, quint8(table_->currentData().toInt()));
            put16(record, 4, quint16(item_->currentData().toInt()));
            next.setTreasure(key_, treasureRow_, record);
        });
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        change("Remove scenario treasure", [this](ScenarioWorld &next) {
            next.removeTreasure(key_, treasureRow_);
        });
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        change("Restore scenario world data",
               [this](ScenarioWorld &next) { next.restoreScenario(key_); });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh(key_);
        changed_();
    });
}
void ScenarioWorldEditor::refresh(int key) {
    if (key != key_) siteRow_ = treasureRow_ = -1;
    key_ = key;
    setEnabled(key_ > 0);
    if (key_ > 0) {
        if (siteRow_ >= project_->world()->siteCount(key_)) siteRow_ = -1;
        if (treasureRow_ >= ScenarioWorld::treasureCount(project_->world()->scenario(key_).treasures))
            treasureRow_ = -1;
    }
    showSite();
    showTreasure();
}
int ScenarioWorldEditor::siteId() const {
    if (key_ <= 0 || siteRow_ < 0 || siteRow_ >= project_->world()->siteCount(key_)) return -1;
    return u16(project_->world()->scenario(key_).sites, 1 + siteRow_ * 4);
}
void ScenarioWorldEditor::showSite() {
    if (loading_)
        return;
    siteFields_->setEnabled(siteId() >= 0);
    if (siteId() < 0)
        return;
    loading_ = true;
    const int id = siteId();
    const auto record = project_->world()->town(id);
    name_->setText(project_->world()->townName(id));
    population_->setValue(u16(record, 22));
    morale_->setValue(u8(record, 24) == 255 ? 50 : u8(record, 24));
    objective_->setChecked(u8(record, 24) == 255);
    morale_->setEnabled(!objective_->isChecked());
    select(type_, u8(record, 26));
    shop_->setMaximum(std::max(project_->shops()->size() - 1, int(u8(record, 27))));
    shop_->setValue(u8(record, 27));
    for (int i = 0; i < 4; ++i)
        facilities_[i]->setChecked(u8(record, 25) & (1 << i));
    const int raw = u16(project_->world()->scenario(key_).sites, 3 + siteRow_ * 4);
    const auto label = raw == 0                                     ? "Neutral"
                       : raw == 4                                   ? "Enemy"
                       : raw == 0x2012 || raw == 0x2002 || raw == 2 ? "Allied"
                                                                    : "Unclassified";
    allegiance_->setItemText(0, QString("Keep current: %1").arg(label));
    allegiance_->setToolTip(
        QString("Stored descriptor: 0x%1. An explicit choice uses the game's standard value.")
            .arg(raw, 4, 16, QChar('0')));
    allegiance_->setCurrentIndex(0);
    siteHelp_->setText(
        QString("Site %1 metadata is shared across scenarios. Initial allegiance applies only to this scenario.")
            .arg(id));
    loading_ = false;
}
void ScenarioWorldEditor::fillItems() {
    item_->clear();
    const int table = table_->currentData().toInt();
    if (table != 1 && table != 2)
        return;
    for (const auto &r : project_->records(table == 1 ? Table::Items : Table::Consumables))
        item_->addItem(QString("%1 · %2").arg(r.id).arg(r.name), r.id);
}
void ScenarioWorldEditor::selectObject(bool treasure, int row) {
    if (treasure) {
        treasureRow_ = row;
        showTreasure();
    } else {
        siteRow_ = row;
        showSite();
    }
    siteFields_->setVisible(!treasure);
    treasureFields_->setVisible(treasure);
    notice_->clear();
}
void ScenarioWorldEditor::showTreasure() {
    if (loading_)
        return;
    treasureFields_->setEnabled(key_ > 0 && treasureRow_ >= 0);
    if (key_ <= 0 || treasureRow_ < 0)
        return;
    loading_ = true;
    const auto r = ScenarioWorld::treasureRecord(project_->world()->scenario(key_).treasures,
                                                 treasureRow_);
    x_->setValue(u8(r, 1));
    z_->setValue(u8(r, 2));
    select(table_, u8(r, 3));
    fillItems();
    select(item_, u16(r, 4));
    flag_->setText(QString("Collection flag %1 · %2 of 16 scene slots used")
                       .arg(u8(r, 0))
                       .arg(ScenarioWorld::treasureCount(project_->world()->scenario(key_).treasures)));
    loading_ = false;
}
void ScenarioWorldEditor::change(const QString &label,
                                 const std::function<void(ScenarioWorld &)> &edit) {
    if (key_ <= 0)
        return;
    try {
        auto next = *project_->world();
        edit(next);
        const auto before = project_->world()->json(), after = next.json();
        if (before != after)
            undo_->push(new WorldCommand(project_, label, before, after));
        notice_->clear();
    } catch (const Error &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
