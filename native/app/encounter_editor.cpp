#include "app/content_refresh.h"
#include "app/view_position.h"
#include "app/encounter_editor.h"
#include "app/custom_squad_editor.h"
#include "app/terrain_assignments.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
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
    split->addWidget(pools_);
    auto details = new QWidget;
    auto detail = new QVBoxLayout(details);
    heading_ = new QLabel;
    heading_->setWordWrap(true);
    detail->addWidget(heading_);
    auto explanation = new QLabel("Edit creatures for the selected map. Terrains grouped together share these choices. "
        "Each outcome has a 50/50 chance after the rate checks; None produces no encounter. "
        "Terrain chances and assignments apply to all maps.");
    explanation->setWordWrap(true);
    detail->addWidget(explanation);
    pairTable_ = new QTableWidget(0, 4);
    pairTable_->setObjectName("encounterPairs");
    pairTable_->setHorizontalHeaderLabels({"Terrain · shared creature choices", "Terrain chance · all maps", "First creature", "Second creature"});
    pairTable_->verticalHeader()->hide();
    pairTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pairTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    pairTable_->setColumnWidth(1, 175);
    pairTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detail->addWidget(pairTable_, 1);
    auto actions = new QHBoxLayout;
    auto copy = new QPushButton("Copy encounters from…");
    copy->setObjectName("encounterCopy");
    copyFrom_ = new QComboBox;
    copyFrom_->setObjectName("encounterCopySource");
    copyFrom_->setMinimumContentsLength(20);
    copyFrom_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto restore = new QPushButton("Restore loaded encounters");
    restore->setObjectName("encounterRestore");
    actions->addWidget(copy);
    actions->addWidget(copyFrom_, 1);
    actions->addWidget(restore);
    detail->addLayout(actions);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details);
    split->setSizes({300, 900});
    tabs->addTab(poolPage, "Map encounters");
    auto ratePage = new QWidget;
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
    connect(back, &QPushButton::clicked, tabs, [tabs] { tabs->setCurrentIndex(0); });
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
    tabs->addTab(ratePage, "Terrain rules · All maps");
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
        copyFrom_->addItem(item->text(), id);
    }
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(pools_, &QListWidget::currentRowChanged, this, [this] { refresh(); });
    connect(copy, &QPushButton::clicked, this, [this] {
        if (selected() < 1)
            return;
        auto next = *project_->encounters();
        const int source = copyFrom_->currentData().toInt();
        for (int slot = 0; slot < project_->encounters()->slotsPerPool(); ++slot)
            next.setPair(selected(), slot, next.classAt(source, slot, 0),
                         next.classAt(source, slot, 1),
                         int(project_->records(Table::Classes).size()));
        commit(next, "Copy encounter pool");
    });
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
    pools_->setCurrentRow(0);
    refresh();
}
int EncounterEditor::selected() const {
    return pools_->currentItem() ? pools_->currentItem()->data(Qt::UserRole).toInt() : -1;
}
void EncounterEditor::refresh() {
    const ViewPosition keepView(this);
    const auto &encounters = *project_->encounters();
    refreshPoolRows();
    const int pool = selected();
    if (pool >= 1) {
        heading_->setText(encounters.label(pool) +
                          (encounters.scenarioKeys(pool).isEmpty()
                               ? "\nNo scenario references in the loaded metadata."
                               : QString()));
        for (int slot = 0; slot < project_->encounters()->slotsPerPool(); ++slot)
            for (int side = 0; side < 2; ++side) {
                auto combo = pairs_[slot * 2 + side];
                QSignalBlocker block(combo);
                combo->clear();
                combo->addItem("0 · None", 0);
                for (int id = 1; id < project_->records(Table::Classes).size(); ++id)
                    combo->addItem(
                        QString("%1 · %2").arg(id).arg(
                            QString(project_->record(Table::Classes, id).name).replace('\n', ' ')),
                        id);
                combo->setCurrentIndex(combo->findData(encounters.classAt(pool, slot, side)));
            }
    }
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
    while (pairs_.size() > count * 2) pairs_.removeLast();
    pairTable_->setRowCount(count);
    for (int slot = 0; slot < count; ++slot) {
        if (!pairTable_->item(slot, 0)) pairTable_->setItem(slot, 0, new QTableWidgetItem);
        QStringList terrainNames, chances, states;
        bool anyChance = false;
        for (int t = 0; t < Encounters::terrainCount; ++t) {
            if (encounters.lookup(t) != slot + 1) continue;
            const auto name = Encounters::mapTerrainName(t);
            terrainNames << name;
            chances << QString("%1: %2%").arg(name).arg(encounters.rate(t));
            anyChance |= encounters.rate(t) > 0;
        }
        if (terrainNames.isEmpty()) states << "No terrain assigned";
        else if (!anyChance) states << "Terrain chance is zero";
        bool custom = false;
        const auto keys = selected() >= 1 ? encounters.scenarioKeys(selected()) : QVector<int>{};
        for (int key : keys) {
            const auto profile = encounters.customSquads().profiles().constFind(CustomSquads::key(key, slot));
            if (profile != encounters.customSquads().profiles().cend() && profile->enabled) {
                custom = true;
                states << QString("Custom squad on runtime key %1").arg(key);
            }
        }
        if (selected() >= 1 && !encounters.classAt(selected(), slot, 0) && !encounters.classAt(selected(), slot, 1))
            states << (custom ? "No base creatures assigned" : "No creatures assigned");
        const auto terrainText = terrainNames.isEmpty() ? QString("Unassigned terrain") : terrainNames.join(" / ");
        const auto subtitle = QString("Pool %1").arg(slot + 1) +
            (terrainNames.size() > 1 ? " · Shared creature choices" : "");
        auto item = pairTable_->item(slot, 0);
        item->setFlags(Qt::ItemIsEnabled);
        item->setText(terrainText + "\n" + subtitle + (states.isEmpty() ? "" : "\n" + states.join("\n")));
        item->setForeground(Qt::transparent); // The rich label paints this cell's visible text.
        item->setToolTip(encounters.poolSlotLabel(slot) + "\n" + states.join("\n"));
        auto terrainLabel = qobject_cast<QLabel *>(pairTable_->cellWidget(slot, 0));
        if (!terrainLabel) {
            terrainLabel = new QLabel;
            terrainLabel->setMargin(7);
            terrainLabel->setWordWrap(true);
            pairTable_->setCellWidget(slot, 0, terrainLabel);
        }
        terrainLabel->setText("<b>" + terrainText.toHtmlEscaped() + "</b><br/><small>" +
            subtitle.toHtmlEscaped() + "</small>" + (states.isEmpty() ? "" : "<br/>" + states.join("\n").toHtmlEscaped().replace('\n', "<br/>")));
        terrainLabel->setToolTip(item->toolTip());
        auto chanceCell = pairTable_->cellWidget(slot, 1);
        if (!chanceCell) {
            chanceCell = new QWidget;
            auto chanceLayout = new QVBoxLayout(chanceCell);
            chanceLayout->setContentsMargins(4, 4, 4, 4);
            chanceLayout->setSpacing(4);
            chanceLayout->addStretch();
            auto chanceText = new QLabel;
            chanceText->setObjectName(QString("encounterTerrainChance_%1").arg(slot));
            chanceText->setAlignment(Qt::AlignCenter);
            chanceText->setWordWrap(true);
            chanceLayout->addWidget(chanceText);
            auto rules = new QPushButton("Edit rules…");
            rules->setObjectName(QString("encounterTerrainRules_%1").arg(slot));
            rules->setToolTip("Edit terrain chances and assignments. Changes affect all maps.");
            chanceLayout->addWidget(rules, 0, Qt::AlignHCenter);
            chanceLayout->addStretch();
            pairTable_->setCellWidget(slot, 1, chanceCell);
            connect(rules, &QPushButton::clicked, this, [this, slot] { editTerrainRules(slot); });
        }
        if (chances.size() == 1) chances[0] = chances[0].section(": ", -1);
        if (!chances.isEmpty() && !anyChance) chances << "Disabled";
        chanceCell->findChild<QLabel *>()->setText(chances.isEmpty() ? "Unassigned" : chances.join('\n'));
        pairTable_->setRowHeight(slot, std::max(3 + int(states.size()), int(chances.size()) + 2) * fontMetrics().lineSpacing() + 14);
        if (pairs_.size() > slot * 2) continue;
        for (int side = 0; side < 2; ++side) {
            auto choice = new QComboBox;
            choice->setObjectName(QString("encounterClass_%1_%2").arg(slot).arg(side));
            choice->setMinimumContentsLength(12);
            choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            choice->setEditable(true); choice->setInsertPolicy(QComboBox::NoInsert);
            choice->completer()->setCompletionMode(QCompleter::PopupCompletion);
            choice->completer()->setFilterMode(Qt::MatchContains);
            choice->completer()->setCaseSensitivity(Qt::CaseInsensitive);
            auto cell = new QWidget;
            auto cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(5, 3, 5, 3);
            cellLayout->addStretch();
            cellLayout->addWidget(choice);
            cellLayout->addStretch();
            pairTable_->setCellWidget(slot, side + 2, cell);
            pairs_ << choice;
            connect(choice, &QComboBox::currentIndexChanged, this, [this, slot] { changePair(slot); });
        }
    }
    const int selected = poolTiles_->currentRow();
    poolTiles_->clear();
    for (int slot = 0; slot < count; ++slot) {
        auto tile = new QListWidgetItem(QString("Pool %1\n%2").arg(slot + 1).arg(poolContents(slot)), poolTiles_);
        tile->setData(Qt::UserRole, slot + 1); tile->setToolTip(encounters.poolSlotLabel(slot));
        tile->setToolTip(tile->text() + "\n" + encounters.poolSlotLabel(slot));
        tile->setSizeHint(QSize(260, fontMetrics().lineSpacing() * 3 + 14)); tile->setBackground(palette().brush(QPalette::Highlight));
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
    for (int key : encounters.scenarioKeys(selected())) {
        const auto profile = encounters.customSquads().profiles().constFind(CustomSquads::key(key, slot));
        if (profile != encounters.customSquads().profiles().cend() && profile->enabled) {
            result += " · Custom squad override";
            break;
        }
    }
    return result;
}
void EncounterEditor::editTerrainRules(int slot) {
    findChild<QTabWidget *>("encounterTabs")->setCurrentIndex(1);
    poolTiles_->setCurrentRow(slot);
    poolTiles_->scrollToItem(poolTiles_->currentItem());
    for (int t = 0; t < Encounters::terrainCount; ++t)
        if (project_->encounters()->lookup(t) == slot + 1) {
            terrainTable_->setCurrentCell(t, 2);
            terrainTable_->scrollToItem(terrainTable_->item(t, 2));
            break;
        }
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
void EncounterEditor::changePair(int slot) {
    if (selected() < 1 || pairs_[slot * 2]->currentIndex() < 0 ||
        pairs_[slot * 2 + 1]->currentIndex() < 0)
        return;
    try {
        auto next = *project_->encounters();
        next.setPair(selected(), slot, pairs_[slot * 2]->currentData().toInt(),
                     pairs_[slot * 2 + 1]->currentData().toInt(),
                     int(project_->records(Table::Classes).size()));
        commit(next, "Edit encounter pair");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
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
