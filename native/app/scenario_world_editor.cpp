#include "app/scenario_world_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
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
    auto layout = new QVBoxLayout(this);
    auto pages = new QTabWidget;
    pages->setObjectName("worldPages");
    layout->addWidget(pages, 1);
    auto siteSplit = new QSplitter;
    pages->addTab(siteSplit, "Sites");
    sites_ = new QListWidget;
    sites_->setObjectName("worldSites");
    siteSplit->addWidget(sites_);
    siteFields_ = new QWidget;
    siteSplit->addWidget(siteFields_);
    auto siteLayout = new QVBoxLayout(siteFields_);
    siteHelp_ = new QLabel;
    siteHelp_->setWordWrap(true);
    siteLayout->addWidget(siteHelp_);
    auto form = new QFormLayout;
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
    form->addRow(objective_);
    type_ = new QComboBox;
    type_->setObjectName("worldSiteType");
    type_->addItem("Town", 9);
    type_->addItem("Fort", 0x29);
    type_->addItem("Boss fort", 0x49);
    type_->addItem("Castle", 0x89);
    form->addRow("Type", type_);
    auto facilityRow = new QHBoxLayout;
    for (const auto &label : {"Shop", "Temple", "Treasure", "Mine"}) {
        auto check = new QCheckBox(label);
        check->setObjectName(QString("worldFacility%1").arg(facilities_.size()));
        facilities_ << check;
        facilityRow->addWidget(check);
    }
    form->addRow("Facilities", facilityRow);
    shop_ = spin("worldShop", 35);
    shop_->setSpecialValueText("None");
    form->addRow("Shop ID", shop_);
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
    siteSplit->setStretchFactor(1, 2);
    auto treasureSplit = new QSplitter;
    pages->addTab(treasureSplit, "Treasure");
    auto roster = new QWidget;
    auto rosterLayout = new QVBoxLayout(roster);
    treasures_ = new QListWidget;
    treasures_->setObjectName("worldTreasures");
    rosterLayout->addWidget(treasures_);
    auto add = new QPushButton("Add treasure");
    add->setObjectName("worldTreasureAdd");
    rosterLayout->addWidget(add);
    auto remove = new QPushButton("Remove treasure");
    remove->setObjectName("worldTreasureRemove");
    rosterLayout->addWidget(remove);
    treasureSplit->addWidget(roster);
    treasureFields_ = new QWidget;
    treasureSplit->addWidget(treasureFields_);
    auto treasureLayout = new QVBoxLayout(treasureFields_);
    auto help = new QLabel(
        "Coordinates use the game's 0–255 map scale. Collection flags are shared across scenarios; "
        "editing an existing treasure retains its flag. New treasures receive an unused flag.");
    help->setWordWrap(true);
    treasureLayout->addWidget(help);
    flag_ = new QLabel;
    treasureLayout->addWidget(flag_);
    auto treasureForm = new QFormLayout;
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
    treasureForm->addRow("Item table", table_);
    treasureForm->addRow("Item", item_);
    auto applyTreasure = new QPushButton("Apply treasure changes");
    applyTreasure->setObjectName("worldTreasureApply");
    treasureLayout->addWidget(applyTreasure);
    treasureLayout->addStretch();
    treasureSplit->setStretchFactor(1, 2);
    auto restore = new QPushButton("Restore loaded treasure and allegiance for this scenario");
    restore->setObjectName("worldScenarioRestore");
    layout->addWidget(restore);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    notice_->setObjectName("worldNotice");
    layout->addWidget(notice_);
    connect(sites_, &QListWidget::currentRowChanged, this, [this] { showSite(); });
    connect(treasures_, &QListWidget::currentRowChanged, this, [this] { showTreasure(); });
    connect(table_, &QComboBox::currentIndexChanged, this, [this] {
        if (!loading_)
            fillItems();
    });
    connect(objective_, &QCheckBox::toggled, this,
            [this](bool value) { morale_->setEnabled(!value); });
    connect(applySite, &QPushButton::clicked, this, [this] {
        if (!sites_->currentItem())
            return;
        change("Edit scenario site", [this](ScenarioWorld &next) {
            const int id = sites_->currentItem()->data(Qt::UserRole).toInt();
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
            put8(record, 26, quint8(type_->currentData().toInt()));
            put8(record, 27, quint8(shop_->value()));
            next.setTown(id, record);
            if (allegiance_->currentData().toInt() >= 0)
                next.setSiteAllegiance(key_, sites_->currentRow(),
                                       allegiance_->currentData().toInt());
        });
    });
    connect(restoreSite, &QPushButton::clicked, this, [this] {
        if (sites_->currentItem())
            change("Restore site metadata", [this](ScenarioWorld &next) {
                next.restoreTown(sites_->currentItem()->data(Qt::UserRole).toInt());
            });
    });
    connect(applyTreasure, &QPushButton::clicked, this, [this] {
        if (treasures_->currentRow() < 0)
            return;
        change("Edit scenario treasure", [this](ScenarioWorld &next) {
            auto record = ScenarioWorld::treasureRecord(next.scenario(key_).treasures,
                                                        treasures_->currentRow());
            put8(record, 1, quint8(x_->value()));
            put8(record, 2, quint8(z_->value()));
            put8(record, 3, quint8(table_->currentData().toInt()));
            put16(record, 4, quint16(item_->currentData().toInt()));
            next.setTreasure(key_, treasures_->currentRow(), record);
        });
    });
    connect(add, &QPushButton::clicked, this, [this] {
        int row = -1;
        change("Add scenario treasure", [&](ScenarioWorld &next) { row = next.addTreasure(key_); });
        if (row >= 0)
            treasures_->setCurrentRow(row);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        change("Remove scenario treasure", [this](ScenarioWorld &next) {
            next.removeTreasure(key_, treasures_->currentRow());
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
    const int sr = key == key_ ? sites_->currentRow() : 0,
              tr = key == key_ ? treasures_->currentRow() : 0;
    key_ = key;
    setEnabled(key_ > 0);
    loading_ = true;
    sites_->clear();
    treasures_->clear();
    if (key_ > 0) {
        const auto &s = project_->world()->scenario(key_);
        for (int i = 0; i < ScenarioWorld::siteCount(s.sites); ++i) {
            const int id = u16(s.sites, 1 + i * 4);
            auto item = new QListWidgetItem(
                QString("%1 · %2").arg(id).arg(project_->world()->townName(id)), sites_);
            item->setData(Qt::UserRole, id);
        }
        for (int i = 0; i < ScenarioWorld::treasureCount(s.treasures); ++i) {
            const auto r = ScenarioWorld::treasureRecord(s.treasures, i);
            const auto &records =
                project_->records(u8(r, 3) == 1 ? Table::Items : Table::Consumables);
            const int id = u16(r, 4);
            const auto name = id < records.size() ? records[id].name : QString("Item %1").arg(id);
            treasures_->addItem(QString("%1 · %2\nX %3, Z %4 · flag %5")
                                    .arg(i + 1)
                                    .arg(name)
                                    .arg(u8(r, 1))
                                    .arg(u8(r, 2))
                                    .arg(u8(r, 0)));
        }
    }
    sites_->setCurrentRow(sites_->count() ? std::clamp(sr, 0, sites_->count() - 1) : -1);
    treasures_->setCurrentRow(treasures_->count() ? std::clamp(tr, 0, treasures_->count() - 1)
                                                  : -1);
    loading_ = false;
    showSite();
    showTreasure();
}
void ScenarioWorldEditor::showSite() {
    if (loading_)
        return;
    siteFields_->setEnabled(sites_->currentItem());
    if (!sites_->currentItem())
        return;
    loading_ = true;
    const int id = sites_->currentItem()->data(Qt::UserRole).toInt();
    const auto record = project_->world()->town(id);
    name_->setText(project_->world()->townName(id));
    population_->setValue(u16(record, 22));
    morale_->setValue(u8(record, 24) == 255 ? 50 : u8(record, 24));
    objective_->setChecked(u8(record, 24) == 255);
    morale_->setEnabled(!objective_->isChecked());
    select(type_, u8(record, 26));
    shop_->setMaximum(std::max(35, int(u8(record, 27))));
    shop_->setValue(u8(record, 27));
    for (int i = 0; i < 4; ++i)
        facilities_[i]->setChecked(u8(record, 25) & (1 << i));
    const int raw = u16(project_->world()->scenario(key_).sites, 3 + sites_->currentRow() * 4);
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
        QString("Metadata belongs to site %1 and affects every scenario using it. Initial "
                "allegiance changes only this scenario. Site membership follows mission geometry.")
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
void ScenarioWorldEditor::showTreasure() {
    if (loading_)
        return;
    treasureFields_->setEnabled(treasures_->currentRow() >= 0);
    if (treasures_->currentRow() < 0)
        return;
    loading_ = true;
    const auto r = ScenarioWorld::treasureRecord(project_->world()->scenario(key_).treasures,
                                                 treasures_->currentRow());
    x_->setValue(u8(r, 1));
    z_->setValue(u8(r, 2));
    select(table_, u8(r, 3));
    fillItems();
    select(item_, u16(r, 4));
    flag_->setText(QString("Collection flag %1 · %2 of 16 scene slots used")
                       .arg(u8(r, 0))
                       .arg(treasures_->count()));
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
