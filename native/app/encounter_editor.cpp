#include "app/content_refresh.h"
#include "app/view_position.h"
#include "app/encounter_editor.h"
#include "app/encounter_labels.h"
#include "app/custom_squad_editor.h"
#include "app/terrain_assignments.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
namespace lordly {
class EncounterCommand final : public QUndoCommand {
  public:
    EncounterCommand(Project *project, QString label, QJsonObject before, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override {
        project_->setEncounters(before_);
    }
    void redo() override {
        project_->setEncounters(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
EncounterEditor::EncounterEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                                 QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("encounterEditor");
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto tabs = new QTabWidget;
    tabs->setObjectName("encounterTabs");
    layout->addWidget(tabs);
    auto poolPage = new QWidget;
    auto poolLayout = new QVBoxLayout(poolPage);
    search_ = new QLineEdit;
    search_->setObjectName("encounterSearch");
    search_->setPlaceholderText("Find a map by name, mission, runtime key, or creature…");
    search_->setClearButtonEnabled(true);
    poolLayout->addWidget(search_);
    auto split = new QSplitter;
    poolLayout->addWidget(split, 1);
    pools_ = new QListWidget;
    pools_->setObjectName("encounterPools");
    pools_->setMinimumWidth(220);
    pools_->setWordWrap(true);
    pools_->setTextElideMode(Qt::ElideNone);
    pools_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    split->addWidget(pools_);
    auto details = new QWidget;
    auto detail = new QVBoxLayout(details);
    heading_ = new QLabel;
    heading_->setWordWrap(true);
    detail->addWidget(heading_);
    auto explanation = new QLabel("Choose a map, select a terrain, then choose creatures or assign a squad. "
        "Encounter contents belong to the selected map; terrain chances and shared terrain groups are global.");
    explanation->setWordWrap(true);
    detail->addWidget(explanation);
    mapKey_ = new QComboBox;
    mapKey_->setObjectName("encounterMapKey");
    auto context = new QHBoxLayout;
    context->addWidget(new QLabel("Map version"));
    context->addWidget(mapKey_, 1);
    auto advanced = new QPushButton("Advanced terrain rules…");
    advanced->setObjectName("encounterAdvancedTerrain");
    context->addWidget(advanced);
    detail->addLayout(context);
    auto showUnassigned = new QCheckBox("Show unassigned terrains");
    showUnassigned->setObjectName("encounterShowUnassigned");
    detail->addWidget(showUnassigned);
    pairTable_ = new QTableWidget(Encounters::terrainCount, 3);
    pairTable_->setObjectName("encounterPairs");
    pairTable_->setHorizontalHeaderLabels({"Terrain", "Chance · all maps", "Encounter on this map"});
    pairTable_->verticalHeader()->hide();
    pairTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pairTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    pairTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    pairTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    pairTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pairTable_->setWordWrap(true);
    for (int t = 0; t < Encounters::terrainCount; ++t)
        for (int col = 0; col < 3; ++col) pairTable_->setItem(t, col, new QTableWidgetItem);
    detail->addWidget(pairTable_, 1);
    selection_ = new QLabel;
    selection_->setObjectName("encounterSelection");
    selection_->setWordWrap(true);
    detail->addWidget(selection_);
    scope_ = new QLabel;
    scope_->setWordWrap(true);
    detail->addWidget(scope_);
    auto encounterActions = new QHBoxLayout;
    for (const auto &entry : {qMakePair("Choose creatures…", "encounterChooseCreatures"),
                              qMakePair("Assign squad…", "encounterAssignSquad"),
                              qMakePair("Edit assigned squad…", "encounterEditSquad")}) {
        auto button = new QPushButton(entry.first);
        button->setObjectName(entry.second);
        encounterActions->addWidget(button);
    }
    detail->addLayout(encounterActions);
    auto actions = new QHBoxLayout;
    auto copy = new QPushButton("Copy from map…");
    copy->setObjectName("encounterCopy");
    auto restore = new QPushButton("Restore loaded creatures");
    restore->setObjectName("encounterRestore");
    restore->setToolTip("Restore creature choices for this map group. Assigned squads are kept.");
    actions->addWidget(copy);
    actions->addStretch();
    actions->addWidget(restore);
    detail->addLayout(actions);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details);
    split->setSizes({300, 900});
    tabs->addTab(poolPage, "Map encounters");
    terrainDialog_ = new QDialog(this);
    terrainDialog_->setObjectName("encounterTerrainDialog");
    terrainDialog_->setWindowTitle("Advanced terrain rules · All maps");
    terrainDialog_->resize(1100, 700);
    auto ratePage = terrainDialog_;
    auto rateLayout = new QVBoxLayout(ratePage);
    auto rateExplanation = new QLabel(
        "These chances and terrain assignments affect ALL maps. Creature previews below follow the map selected in Map encounters. "
        "Drag from Available pools to share a pool; drag between terrain rows to move or swap assignments.");
    rateExplanation->setWordWrap(true);
    rateLayout->addWidget(rateExplanation);
    auto contextRow = new QHBoxLayout;
    terrainContext_ = new QLabel;
    terrainContext_->setObjectName("terrainMapContext");
    terrainContext_->setWordWrap(true);
    contextRow->addWidget(terrainContext_, 1);
    auto back = new QPushButton("Back to map encounters");
    back->setObjectName("encounterBackToMap");
    contextRow->addWidget(back);
    connect(back, &QPushButton::clicked, terrainDialog_, &QDialog::hide);
    rateLayout->addLayout(contextRow);
    auto rateSplit = new QSplitter;
    auto rateTable = new TerrainAssignments;
    terrainTable_ = rateTable;
    rateTable->setObjectName("terrainAssignments");
    rateTable->setHorizontalHeaderLabels({"Map terrain (fixed)", "Terrain chance (%)", "Assigned pool · selected map's creatures"});
    rateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rateTable->verticalHeader()->hide();
    for (int terrain = 0; terrain < Encounters::terrainCount; ++terrain) {
        rateTable->setItem(terrain, 0, new QTableWidgetItem(
            QString("%1 · %2").arg(terrain).arg(Encounters::mapTerrainName(terrain))));
        rateTable->item(terrain, 0)->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        auto rate = new QSpinBox;
        rate->setObjectName(QString("encounterRate_%1").arg(terrain));
        rate->setRange(0, 100);
        rateTable->setCellWidget(terrain, 1, rate);
        rateTable->setItem(terrain, 2, new QTableWidgetItem);
        rateTable->setRowHeight(terrain, fontMetrics().lineSpacing() * 2 + 18);
        rates_ << rate;
        connect(rate, &QSpinBox::valueChanged, this, [this, terrain] { changeTerrain(terrain); });
    }
    rateTable->assign = [this](int from, int to, int pool) {
        try {
            auto next = *project_->encounters();
            if (to < 0 || to >= 32 || from >= 32 || pool < 1 || pool > next.slotsPerPool()) return;
            if (from == to) return;
            if (from >= 0) {
                if (next.lookup(from) != pool) return;
                const int other = next.lookup(to);
                next.setTerrain(from, other ? next.rate(from) : 0, other);
            }
            next.setTerrain(to, next.rate(to), pool);
            commit(next, "Move encounter pool assignment");
            terrainTable_->setCurrentCell(to, 2);
        } catch (const Error &e) { notice_->setText(QString::fromUtf8(e.what())); }
    };
    rateSplit->addWidget(rateTable);
    auto palette = new QWidget;
    auto paletteLayout = new QVBoxLayout(palette);
    paletteLayout->addWidget(new QLabel("Available pools — drag to assign"));
    poolTiles_ = new PoolTiles;
    poolTiles_->setObjectName("encounterPoolTiles");
    poolTiles_->setWordWrap(true);
    poolTiles_->setSpacing(5);
    paletteLayout->addWidget(poolTiles_, 1);
    auto addPool = new QPushButton("Add pool");
    addPool->setObjectName("encounterAddPool");
    paletteLayout->addWidget(addPool);
    auto clearPool = new QPushButton("Clear selected terrain assignment");
    clearPool->setObjectName("encounterClearPool");
    paletteLayout->addWidget(clearPool);
    connect(addPool, &QPushButton::clicked, this, [this] {
        auto next = *project_->encounters();
        const int slot = next.addPool();
        const int row = terrainTable_->currentRow();
        if (row >= 0 && !next.lookup(row)) next.setTerrain(row, 0, slot + 1);
        commit(next, "Add encounter pool");
        poolTiles_->setCurrentRow(slot);
    });
    connect(clearPool, &QPushButton::clicked, this, [this] {
        const int row = terrainTable_->currentRow();
        if (row < 0) return;
        auto next = *project_->encounters(); next.setTerrain(row, 0, 0);
        commit(next, "Clear encounter pool assignment");
    });
    rateSplit->addWidget(palette); rateSplit->setSizes({900, 320});
    rateLayout->addWidget(rateSplit, 1);
    connect(advanced, &QPushButton::clicked, this, [this] { editTerrainRules(selectedSlot()); });
    connect(pairTable_, &QTableWidget::currentCellChanged, this, [this] { showTerrain(); });
    connect(mapKey_, &QComboBox::currentIndexChanged, this, [this] { refreshPoolRows(); });
    connect(showUnassigned, &QCheckBox::toggled, this, [this] { refreshPoolRows(); });
    connect(findChild<QPushButton *>("encounterChooseCreatures"), &QPushButton::clicked, this, &EncounterEditor::chooseCreatures);
    connect(findChild<QPushButton *>("encounterAssignSquad"), &QPushButton::clicked, this, &EncounterEditor::assignSquad);
    connect(findChild<QPushButton *>("encounterEditSquad"), &QPushButton::clicked, this, &EncounterEditor::editSquad);
    auto globalPage = new QWidget;
    auto globalLayout = new QVBoxLayout(globalPage);
    auto globalExplanation = new QLabel(
        "The first encounter check draws a value below the number of possible outcomes. "
        "It passes when the value is below the successful-outcome count. The game selects "
        "the In Mission or In Area investigation count. Terrain rates and unit eligibility "
        "still apply afterward.");
    globalExplanation->setWordWrap(true);
    globalLayout->addWidget(globalExplanation);
    auto form = new QFormLayout;
    divisor_ = new QDoubleSpinBox;
    normalPass_ = new QDoubleSpinBox;
    alternatePass_ = new QDoubleSpinBox;
    divisor_->setObjectName("encounterDivisor");
    normalPass_->setObjectName("encounterNormalPass");
    alternatePass_->setObjectName("encounterAlternatePass");
    for (auto spin : {divisor_, normalPass_, alternatePass_}) {
        spin->setDecimals(0);
        spin->setRange(0, 4294967295.0);
        spin->setSingleStep(1);
        spin->setGroupSeparatorShown(true);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { showGlobalRatio(); });
    }
    divisor_->setMinimum(1);
    form->addRow("Possible outcomes", divisor_);
    form->addRow("In Mission — successful outcomes", alternatePass_);
    form->addRow("In Area investigation — successful outcomes", normalPass_);
    globalLayout->addLayout(form);
    linkedRates_ = new QCheckBox("Use the Area Investigation count for both contexts");
    linkedRates_->setObjectName("encounterLinkRates");
    globalLayout->addWidget(linkedRates_);
    globalRatio_ = new QLabel;
    globalRatio_->setWordWrap(true);
    globalRatio_->setObjectName("encounterGlobalRatio");
    globalLayout->addWidget(globalRatio_);
    auto globalActions = new QHBoxLayout;
    auto applyGlobal = new QPushButton("Apply global rates");
    applyGlobal->setObjectName("encounterApplyGlobal");
    auto restoreGlobal = new QPushButton("Restore loaded global rates");
    restoreGlobal->setObjectName("encounterRestoreGlobal");
    globalActions->addWidget(applyGlobal);
    globalActions->addWidget(restoreGlobal);
    globalActions->addStretch();
    globalLayout->addLayout(globalActions);
    globalNotice_ = new QLabel;
    globalNotice_->setObjectName("encounterGlobalNotice");
    globalNotice_->setWordWrap(true);
    globalLayout->addWidget(globalNotice_);
    auto assumptions = new QHBoxLayout;
    deployed_ = new QSpinBox;
    deployed_->setObjectName("encounterDeployed"); deployed_->setRange(0, 10); deployed_->setValue(1);
    checks_ = new QDoubleSpinBox;
    checks_->setObjectName("encounterChecksPerSecond"); checks_->setRange(0.1, 240); checks_->setValue(30);
    assumptions->addWidget(new QLabel("Moving, eligible squads")); assumptions->addWidget(deployed_);
    assumptions->addWidget(new QLabel("Assumed checks / second")); assumptions->addWidget(checks_);
    assumptions->addStretch(); globalLayout->addLayout(assumptions);
    auto timingHelp = new QLabel("Estimated mean wait for the rate gates to pass, with all selected squads "
                                "moving on the same terrain. The global roll is shared; each eligible squad "
                                "then rolls terrain chance. Checks/second is an adjustable assumption, not a "
                                "measured game cadence. Pauses, scenario overrides, empty creature choices "
                                "and other eligibility checks can lengthen or prevent actual encounters.");
    timingHelp->setWordWrap(true); globalLayout->addWidget(timingHelp);
    estimates_ = new QTableWidget(32, 3);
    estimates_->setObjectName("encounterWaitEstimates");
    estimates_->setHorizontalHeaderLabels({"Map terrain", "In Mission — mean wait", "Area Investigation — mean wait"});
    estimates_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    estimates_->verticalHeader()->hide(); estimates_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int row = 0; row < 32; ++row)
        for (int col = 0; col < 3; ++col) estimates_->setItem(row, col, new QTableWidgetItem);
    globalLayout->addWidget(estimates_, 1);
    connect(deployed_, &QSpinBox::valueChanged, this, [this] { showGlobalRatio(); });
    connect(checks_, &QDoubleSpinBox::valueChanged, this, [this] { showGlobalRatio(); });
    connect(linkedRates_, &QCheckBox::toggled, this, [this](bool linked) {
        alternatePass_->setEnabled(!linked);
        if (linked)
            alternatePass_->setValue(normalPass_->value());
        showGlobalRatio();
    });
    connect(normalPass_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (linkedRates_->isChecked())
            alternatePass_->setValue(value);
    });
    connect(applyGlobal, &QPushButton::clicked, this, [this] { applyGlobalRate(false); });
    connect(restoreGlobal, &QPushButton::clicked, this, [this] { applyGlobalRate(true); });
    tabs->addTab(globalPage, "Global rates");
    tabs->addTab(createRewardsPage(), "Class and squad rewards");
    tabs->addTab(createScenarioPage(), "Scenario rates");
    tabs->addTab(new CustomSquadEditor(
                     project_, undo_,
                     [this](const Encounters &next, const QString &label) { commit(next, label); }),
                 "Custom squads");
    for (int id = 1; id <= Encounters::poolCount; ++id) {
        auto item = new QListWidgetItem(project_->encounters()->label(id), pools_);
        item->setToolTip(item->text());
        item->setData(Qt::UserRole, id);

    }
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(pools_, &QListWidget::currentRowChanged, this, [this] { refresh(); });
    connect(copy, &QPushButton::clicked, this, &EncounterEditor::copyEncounters);
    connect(restore, &QPushButton::clicked, this, [this] {
        if (selected() < 1)
            return;
        auto next = *project_->encounters();
        const auto original =
            Encounters::load(project_->rom(), int(project_->records(Table::Classes).size()));
        for (int slot = 0; slot < project_->encounters()->slotsPerPool(); ++slot)
            next.setPair(selected(), slot, (slot < original.slotsPerPool() ? original.classAt(selected(), slot, 0) : 0),
                         (slot < original.slotsPerPool() ? original.classAt(selected(), slot, 1) : 0),
                         int(project_->records(Table::Classes).size()));
        commit(next, "Restore encounter pool");
    });
    watchContentChanges(undo_, this, [this] {
        refresh();
        filter();
        changed_();
    });
    pairTable_->setCurrentCell(1, 0);
    pools_->setCurrentRow(0);
    refresh();
}
int EncounterEditor::selected() const {
    return pools_->currentItem() ? pools_->currentItem()->data(Qt::UserRole).toInt() : -1;
}
void EncounterEditor::refresh() {
    const ViewPosition keepView(this);
    const auto &encounters = *project_->encounters();
    const int pool = selected();
    const int previousKey = selectedKey();
    {
        QSignalBlocker block(mapKey_);
        mapKey_->clear();
        if (pool >= 1)
            for (int key : encounters.scenarioKeys(pool))
                mapKey_->addItem(QString("%1 · %2").arg(key).arg(encounters.scenarioLabel(key)), key);
        mapKey_->setCurrentIndex(qMax(0, mapKey_->findData(previousKey)));
        mapKey_->setEnabled(mapKey_->count() > 1);
    }
    heading_->setText(pool >= 1 ? encounters.label(pool) : "Choose a map");
    refreshPoolRows();
    for (int t = 0; t < Encounters::terrainCount; ++t) {
        QSignalBlocker rateBlock(rates_[t]);
        rates_[t]->setValue(encounters.rate(t));
        rates_[t]->setEnabled(encounters.lookup(t) != 0);
        auto tile = terrainTable_->item(t, 2);
        tile->setText(encounters.lookup(t) ? QString("Pool %1\n%2").arg(encounters.lookup(t))
            .arg(poolContents(encounters.lookup(t) - 1)) : "Drop a pool here");
        tile->setToolTip(tile->text());
        tile->setData(Qt::UserRole, encounters.lookup(t));
        tile->setBackground(encounters.lookup(t) ? palette().brush(QPalette::Highlight) : QBrush(Qt::transparent));
        tile->setForeground(palette().brush(encounters.lookup(t) ? QPalette::HighlightedText : QPalette::Text));
        terrainTable_->setRowHeight(t, std::max(2, int(tile->text().count('\n')) + 1) * fontMetrics().lineSpacing() + 18);
    }
    terrainContext_->setText(pool >= 1 ? "Creature preview: " + encounters.label(pool)
                                      : "Select a map in Map encounters to preview creatures.");
    notice_->clear();
    const auto &global = encounters.globalRate();
    QSignalBlocker divisorBlock(divisor_), normalBlock(normalPass_), alternateBlock(alternatePass_),
        linkedBlock(linkedRates_);
    divisor_->setValue(global.divisor());
    normalPass_->setValue(global.normalPassCount());
    alternatePass_->setValue(global.alternatePassCount());
    if (global.normalPassCount() != global.alternatePassCount())
        linkedRates_->setChecked(false);
    alternatePass_->setEnabled(!linkedRates_->isChecked());
    showGlobalRatio();
    globalNotice_->clear();
    refreshRewardClasses();
    refreshScenarios();
}
void EncounterEditor::refreshPoolRows() {
    const auto &encounters = *project_->encounters();
    const int count = encounters.slotsPerPool();
    for (int t = 0; t < Encounters::terrainCount; ++t) {
        const int slot = encounters.lookup(t) - 1;
        pairTable_->setRowHidden(t, slot < 0 && !findChild<QCheckBox *>("encounterShowUnassigned")->isChecked());
        pairTable_->item(t, 0)->setText(Encounters::mapTerrainName(t));
        pairTable_->item(t, 1)->setText(slot < 0 ? "Disabled" : QString("%1%").arg(encounters.rate(t)));
        QString summary = "Disabled — no terrain group assigned";
        if (slot >= 0 && selected() >= 1) {
            const auto &profiles = encounters.customSquads().profiles();
            const int key = selectedKey() < 0 ? -1 : CustomSquads::key(selectedKey(), slot);
            auto it = profiles.constFind(key);
            if (it != profiles.cend() && it->enabled)
                summary = QString("Squad: %1 · %2 members").arg(encounterSquadName(*project_, key)).arg(it->members.size());
            else {
                const int first = encounters.classAt(selected(), slot, 0), second = encounters.classAt(selected(), slot, 1);
                auto name = [this](int id) { return id ? project_->record(Table::Classes, id).name.simplified() : QString("None"); };
                summary = !first && !second ? "No encounter assigned" : first == second ? "Creature: " + name(first)
                    : QString("Creatures: %1 / %2 (50/50)").arg(name(first), name(second));
                if (it != profiles.cend()) summary += "\nSaved squad is disabled";
            }
            if (!encounters.rate(t)) summary += "\nDisabled — terrain chance is 0%";
        }
        pairTable_->item(t, 2)->setText(summary);
        pairTable_->item(t, 2)->setToolTip(summary);
    }
    pairTable_->resizeRowsToContents();
    for (int t = 0; t < Encounters::terrainCount; ++t)
        pairTable_->setRowHeight(t, qMax(42, pairTable_->rowHeight(t)));
    if (pairTable_->currentRow() < 0 || pairTable_->isRowHidden(pairTable_->currentRow()))
        for (int t = 0; t < Encounters::terrainCount; ++t)
            if (!pairTable_->isRowHidden(t)) { pairTable_->setCurrentCell(t, 0); break; }
    showTerrain();
    const int selected = poolTiles_->currentRow();
    poolTiles_->clear();
    for (int slot = 0; slot < count; ++slot) {
        auto tile = new QListWidgetItem(QString("Pool %1\n%2").arg(slot + 1).arg(poolContents(slot)), poolTiles_);
        tile->setData(Qt::UserRole, slot + 1); tile->setToolTip(encounters.poolSlotLabel(slot));
        tile->setToolTip(tile->text() + "\n" + encounters.poolSlotLabel(slot));
        tile->setSizeHint(QSize(260, fontMetrics().lineSpacing() * std::max(3, int(tile->text().count('\n')) + 2) + 14)); tile->setBackground(palette().brush(QPalette::Highlight));
        tile->setForeground(palette().brush(QPalette::HighlightedText));
    }
    poolTiles_->setCurrentRow(qBound(0, selected, count - 1));
    findChild<QPushButton *>("encounterAddPool")->setEnabled(count < 32);
}
QString EncounterEditor::poolContents(int slot) const {
    if (selected() < 1) return "Select a map to preview creatures";
    const auto &encounters = *project_->encounters();
    const int first = encounters.classAt(selected(), slot, 0), second = encounters.classAt(selected(), slot, 1);
    auto name = [this](int id) { return id ? QString(project_->record(Table::Classes, id).name).replace('\n', ' ') : QString("None"); };
    QString result = !first && !second ? "No creatures assigned" : first == second ? name(first) : name(first) + " / " + name(second);
    const auto squads = squadSummaries(slot);
    if (!squads.isEmpty()) {
        if (squads.size() == encounters.scenarioKeys(selected()).size()) return squads.join('\n');
        return squads.join('\n') + "\nOther map keys: " + result;
    }
    return result;
}
QStringList EncounterEditor::squadSummaries(int slot) const {
    QStringList result;
    if (selected() < 1) return result;
    const auto &encounters = *project_->encounters();
    const auto keys = encounters.scenarioKeys(selected());
    for (int key : keys) {
        const auto profile = encounters.customSquads().profiles().constFind(CustomSquads::key(key, slot));
        if (profile != encounters.customSquads().profiles().cend() && profile->enabled) {
            auto summary = QString("Assigned squad: %1 · %2 members")
                .arg(encounterSquadName(*project_, profile.key())).arg(profile->members.size());
            if (keys.size() > 1) summary += QString(" (key %1)").arg(key);
            result << summary;
        }
    }
    return result;
}
void EncounterEditor::editTerrainRules(int slot) {
    terrainDialog_->show();
    terrainDialog_->raise();
    poolTiles_->setCurrentRow(qMax(0, slot));
    poolTiles_->scrollToItem(poolTiles_->currentItem());
    for (int t = 0; t < Encounters::terrainCount; ++t)
        if (project_->encounters()->lookup(t) == slot + 1) {
            terrainTable_->setCurrentCell(t, 2);
            terrainTable_->scrollToItem(terrainTable_->item(t, 2));
            break;
        }
}
int EncounterEditor::selectedKey() const {
    return mapKey_->currentIndex() >= 0 ? mapKey_->currentData().toInt() : -1;
}
int EncounterEditor::selectedSlot() const {
    const int terrain = pairTable_->currentRow();
    return terrain >= 0 ? project_->encounters()->lookup(terrain) - 1 : -1;
}
void EncounterEditor::showTerrain() {
    const int slot = selectedSlot(), terrain = pairTable_->currentRow();
    const bool valid = selected() >= 1 && slot >= 0;
    findChild<QPushButton *>("encounterChooseCreatures")->setEnabled(valid);
    findChild<QPushButton *>("encounterAssignSquad")->setEnabled(valid && selectedKey() >= 0);
    const bool hasSquad = valid && selectedKey() >= 0 && project_->encounters()->customSquads().profiles().contains(CustomSquads::key(selectedKey(), slot));
    findChild<QPushButton *>("encounterEditSquad")->setEnabled(hasSquad);
    selection_->setText(terrain < 0 ? "Select a terrain" : Encounters::mapTerrainName(terrain) + " — " + pairTable_->item(terrain, 2)->text());
    if (!valid) {
        scope_->setText("Enable this terrain in Advanced terrain rules. Those rules affect all maps.");
        return;
    }
    QStringList terrains;
    for (int t = 0; t < Encounters::terrainCount; ++t)
        if (project_->encounters()->lookup(t) == slot + 1) terrains << Encounters::mapTerrainName(t);
    scope_->setText("Applies to: " + terrains.join(", ") + ". " +
        (project_->encounters()->scenarioKeys(selected()).size() > 1
            ? "Creature choices are shared by every map version in this group. Squads apply only to the selected version."
            : "Creature choices and squads apply to this map.") +
        " Terrain chance is one of several encounter checks; it is not the overall encounter frequency.");
}
void EncounterEditor::chooseCreatures() {
    const int slot = selectedSlot(), group = selected(), map = selectedKey();
    if (slot < 0 || group < 1) return;
    QDialog dialog(this);
    dialog.setObjectName("encounterCreatureDialog");
    dialog.setWindowTitle("Choose creatures");
    dialog.resize(560, 220);
    auto layout = new QVBoxLayout(&dialog);
    auto help = new QLabel(scope_->text() + "\nEach choice has a 50/50 chance after the encounter checks. Choose the same creature twice for one guaranteed choice; None means no encounter.");
    help->setWordWrap(true); layout->addWidget(help);
    QComboBox *choices[2];
    auto form = new QFormLayout;
    for (int side = 0; side < 2; ++side) {
        auto choice = choices[side] = new QComboBox;
        choice->setObjectName(QString("encounterCreature_%1").arg(side));
        choice->setEditable(true); choice->setInsertPolicy(QComboBox::NoInsert);
        choice->completer()->setCompletionMode(QCompleter::PopupCompletion);
        choice->completer()->setFilterMode(Qt::MatchContains);
        choice->completer()->setCaseSensitivity(Qt::CaseInsensitive);
        for (int id = 0; id < project_->records(Table::Classes).size(); ++id)
            choice->addItem(QString("%1 · %2").arg(id).arg(id ? project_->record(Table::Classes, id).name.simplified() : "None"), id);
        choice->setCurrentIndex(choice->findData(project_->encounters()->classAt(group, slot, side)));
        form->addRow(side ? "Second choice" : "First choice", choice);
    }
    layout->addLayout(form);
    auto useCreatures = new QCheckBox("Use these creatures instead of the assigned squad on this map version");
    useCreatures->setObjectName("encounterUseCreatures");
    const bool hasSquad = map >= 0 && project_->encounters()->customSquads().profiles().contains(CustomSquads::key(map, slot));
    useCreatures->setVisible(hasSquad); useCreatures->setChecked(hasSquad);
    layout->addWidget(useCreatures);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        auto next = *project_->encounters();
        next.setPair(group, slot, choices[0]->currentData().toInt(), choices[1]->currentData().toInt(), int(project_->records(Table::Classes).size()));
        if (hasSquad && useCreatures->isChecked()) {
            auto squad = next.customSquads().profiles().value(CustomSquads::key(map, slot));
            squad.enabled = false;
            next.setCustomSquad(squad, int(project_->records(Table::Classes).size()));
        }
        commit(next, "Choose map encounter creatures");
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
}
void EncounterEditor::editSquad() {
    if (selectedSlot() < 0 || selectedKey() < 0) return;
    auto editor = static_cast<CustomSquadEditor *>(findChild<QWidget *>("customSquadsPage"));
    editor->editAssignment(selectedKey(), selectedSlot());
    findChild<QTabWidget *>("encounterTabs")->setCurrentWidget(editor);
}
void EncounterEditor::assignSquad() {
    const int slot = selectedSlot(), map = selectedKey();
    if (slot < 0 || map < 0) return;
    QDialog dialog(this);
    dialog.setObjectName("encounterSquadDialog");
    dialog.setWindowTitle("Assign a squad to this map");
    dialog.resize(600, 220);
    auto layout = new QVBoxLayout(&dialog);
    auto help = new QLabel(scope_->text() + "\nAn assigned squad replaces the two creature choices. Copying a squad creates an independent assignment here, including its formation, equipment and rewards.");
    help->setWordWrap(true); layout->addWidget(help);
    auto choice = new QComboBox;
    choice->setObjectName("encounterSquadChoice");
    choice->addItem("Create / edit squad for this map…", -1);
    const auto &profiles = project_->encounters()->customSquads().profiles();
    for (auto it = profiles.cbegin(); it != profiles.cend(); ++it)
        choice->addItem(QString("%1 · %2 members · map %3 · %4%5")
            .arg(encounterSquadName(*project_, it.key())).arg(it->members.size()).arg(it->scenarioKey)
            .arg(project_->encounters()->poolSlotLabel(it->terrainSlot), it->enabled ? "" : " · disabled at source"), it.key());
    layout->addWidget(choice);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    if (choice->currentData().toInt() < 0) { editSquad(); return; }
    try {
        auto next = *project_->encounters();
        auto squad = next.customSquads().profiles().value(choice->currentData().toInt());
        squad.scenarioKey = map; squad.terrainSlot = slot; squad.enabled = true;
        next.setCustomSquad(squad, int(project_->records(Table::Classes).size()));
        next.customSquads().validateFormations(project_->classData());
        commit(next, "Assign encounter squad to map");
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
}
void EncounterEditor::copyEncounters() {
    const int destination = selected(), targetKey = selectedKey();
    if (destination < 1) return;
    QDialog dialog(this);
    dialog.setObjectName("encounterCopyDialog");
    dialog.setWindowTitle("Copy encounters from map");
    dialog.resize(600, 230);
    auto layout = new QVBoxLayout(&dialog);
    auto source = new QComboBox;
    source->setObjectName("encounterCopySource");
    for (int id = 1; id <= Encounters::poolCount; ++id) source->addItem(project_->encounters()->label(id), id);
    layout->addWidget(new QLabel("Copy creature choices from:")); layout->addWidget(source);
    auto include = new QCheckBox("Include squads — replace this map version's squad assignments too");
    include->setObjectName("encounterCopySquads");
    include->setEnabled(targetKey >= 0);
    layout->addWidget(include);
    auto sourceKey = new QComboBox;
    sourceKey->setObjectName("encounterCopyMapKey"); sourceKey->setEnabled(false);
    auto populate = [this, source, sourceKey] {
        sourceKey->clear();
        for (int key : project_->encounters()->scenarioKeys(source->currentData().toInt()))
            sourceKey->addItem(QString("%1 · %2").arg(key).arg(project_->encounters()->scenarioLabel(key)), key);
    };
    connect(source, &QComboBox::currentIndexChanged, &dialog, populate); populate();
    connect(include, &QCheckBox::toggled, sourceKey, &QComboBox::setEnabled);
    layout->addWidget(sourceKey);
    auto help = new QLabel("Unchecked: copy creatures and keep existing squads.\nChecked: copy squads from the selected source version; terrains without a source squad lose their destination squad. Formation, equipment, enabled state and rewards are copied.\nCreature choices update every map version in the destination group. Global terrain rules and rates stay unchanged.");
    help->setWordWrap(true); layout->addWidget(help);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    auto valid = [=] { buttons->button(QDialogButtonBox::Ok)->setEnabled(!include->isChecked() || sourceKey->currentIndex() >= 0); };
    connect(sourceKey, &QComboBox::currentIndexChanged, &dialog, valid);
    connect(include, &QCheckBox::toggled, &dialog, valid);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    try {
        const auto before = *project_->encounters();
        auto next = before;
        for (int slot = 0; slot < next.slotsPerPool(); ++slot) {
            next.setPair(destination, slot, before.classAt(source->currentData().toInt(), slot, 0), before.classAt(source->currentData().toInt(), slot, 1), int(project_->records(Table::Classes).size()));
            if (include->isChecked()) {
                const auto it = before.customSquads().profiles().constFind(CustomSquads::key(sourceKey->currentData().toInt(), slot));
                next.removeCustomSquad(targetKey, slot);
                if (it != before.customSquads().profiles().cend()) {
                    auto squad = it.value(); squad.scenarioKey = targetKey;
                    next.setCustomSquad(squad, int(project_->records(Table::Classes).size()));
                }
            }
        }
        commit(next, include->isChecked() ? "Copy map creatures and squads" : "Copy map creatures");
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
}
void EncounterEditor::showGlobalRatio() {
    // Construction sets ranges before the remaining controls exist.
    if (!globalRatio_)
        return;
    const auto divisor = quint32(divisor_->value()), normal = quint32(normalPass_->value()),
               alternate = quint32(alternatePass_->value());
    if (normal > divisor || alternate > divisor) {
        globalRatio_->setText("Successful outcomes cannot exceed the possible-outcome count.");
        if (estimates_)
            for (int row = 0; row < 32; ++row)
                for (int col = 1; col < 3; ++col) estimates_->item(row, col)->setText("Invalid rate");
        return;
    }
    globalRatio_->setText(
        QString("Share of random values passing: Area Investigation %1%; In Mission %2%.\n"
                "Zero successful outcomes disables that roll. Matching the possible "
                "outcome count passes this check every time. These figures account for the "
                "game's 30-bit random-number range; actual encounter frequency also depends "
                "on the later checks.")
            .arg(100.0 * EncounterRate::passingFraction(divisor, normal), 0, 'f', 8)
            .arg(100.0 * EncounterRate::passingFraction(divisor, alternate), 0, 'f', 8));
    if (!estimates_) return;
    const auto &encounters = *project_->encounters();
    for (int terrain = 0; terrain < 32; ++terrain) {
        estimates_->item(terrain, 0)->setText(QString("%1 · %2").arg(terrain).arg(Encounters::mapTerrainName(terrain)));
        const double local = encounters.lookup(terrain) ? EncounterRate::passingFraction(100, encounters.rate(terrain)) : 0;
        const double anySquad = 1 - std::pow(1 - local, deployed_->value());
        for (int context = 0; context < 2; ++context) {
            const double probability = EncounterRate::passingFraction(divisor, context ? normal : alternate) * anySquad;
            const double seconds = probability > 0 ? 1 / (checks_->value() * probability) : 0;
            const QString wait = !seconds ? "Never" : seconds < 60 ? QString("%1 s").arg(seconds, 0, 'f', 1)
                                : seconds < 3600 ? QString("%1 min").arg(seconds / 60, 0, 'f', 1)
                                                 : QString("%1 h").arg(seconds / 3600, 0, 'f', 1);
            estimates_->item(terrain, context + 1)->setText(wait);
            estimates_->item(terrain, context + 1)->setData(Qt::UserRole, seconds);
        }
    }
}
void EncounterEditor::applyGlobalRate(bool restore) {
    try {
        auto next = *project_->encounters();
        next.setGlobalRate(restore ? EncounterRate::load(project_->rom()).json()
                                   : QJsonObject{{"divisor", divisor_->value()},
                                                 {"normalPassCount", normalPass_->value()},
                                                 {"alternatePassCount", alternatePass_->value()}});
        commit(next, restore ? "Restore global encounter rates" : "Edit global encounter rates");
        refresh();
    } catch (const std::exception &e) {
        globalNotice_->setText(QString::fromUtf8(e.what()));
    }
}
void EncounterEditor::filter() {
    const auto query = search_->text();
    for (int row = 0; row < pools_->count(); ++row) {
        auto item = pools_->item(row);
        const int pool = item->data(Qt::UserRole).toInt();
        QString searchable = item->text();
        for (const int key : project_->encounters()->scenarioKeys(pool))
            searchable += " " + QString::number(key);
        for (int slot = 0; slot < project_->encounters()->slotsPerPool(); ++slot)
            for (int side = 0; side < 2; ++side)
                searchable += " " + project_
                                        ->record(Table::Classes,
                                                 project_->encounters()->classAt(pool, slot, side))
                                        .name;
        item->setHidden(!searchable.contains(query, Qt::CaseInsensitive));
    }
}
void EncounterEditor::commit(const Encounters &next, const QString &label) {
    const auto before = project_->encounters()->json(), after = next.json();
    if (before != after)
        undo_->push(new EncounterCommand(project_, label, before, after));
}
void EncounterEditor::changeTerrain(int terrain) {
    try {
        auto next = *project_->encounters();
        const int slot = next.lookup(terrain);
        next.setTerrain(terrain, slot ? rates_[terrain]->value() : 0, slot);
        commit(next, "Edit encounter terrain rate");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
