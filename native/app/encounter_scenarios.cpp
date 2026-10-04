#include "app/encounter_editor.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
namespace lordly {
QWidget *EncounterEditor::createScenarioPage() {
    auto page = new QWidget;
    page->setObjectName("scenarioRatesPage");
    auto layout = new QVBoxLayout(page);
    auto intro = new QLabel(
        "Override the first encounter roll for one scenario, even when it "
        "shares a creature pool with another mission. Inherit follows future global-rate edits. "
        "Terrain and unit checks still apply after this roll passes. Scenario keys are game "
        "data IDs; unnamed keys may not be used by a playable mission.");
    intro->setWordWrap(true);
    layout->addWidget(intro);
    scenarioSearch_ = new QLineEdit;
    scenarioSearch_->setObjectName("scenarioRateSearch");
    scenarioSearch_->setPlaceholderText("Find a scenario by name or key…");
    scenarioSearch_->setClearButtonEnabled(true);
    layout->addWidget(scenarioSearch_);
    auto split = new QSplitter;
    layout->addWidget(split, 1);
    scenarios_ = new QListWidget;
    scenarios_->setObjectName("scenarioRateKeys");
    split->addWidget(scenarios_);
    scenarioDetails_ = new QWidget;
    auto detail = new QVBoxLayout(scenarioDetails_);
    auto table = new QTableWidget(2, 4);
    table->setHorizontalHeaderLabels(
        {"Mode", "Possible outcomes", "Successful outcomes", "Passing share"});
    table->setVerticalHeaderLabels({"During mission", "Area investigation"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setMaximumHeight(140);
    detail->addWidget(table);
    for (int normal = 0; normal < 2; ++normal) {
        table->setRowHeight(normal, 38);
        auto mode = new QComboBox;
        mode->setObjectName(QString("scenarioMode_%1").arg(normal));
        mode->addItems({"Inherit global", "Override", "Disabled"});
        table->setCellWidget(normal, 0, mode);
        scenarioModes_ << mode;
        for (int which = 0; which < 2; ++which) {
            auto input = new QDoubleSpinBox;
            input->setDecimals(0);
            input->setRange(which ? 0 : 1, 4294967295.0);
            input->setObjectName(
                QString(which ? "scenarioPass_%1" : "scenarioDivisor_%1").arg(normal));
            table->setCellWidget(normal, which + 1, input);
            (which ? scenarioPasses_ : scenarioDivisors_) << input;
            connect(input, &QDoubleSpinBox::valueChanged, this, [this] { showScenarioOdds(); });
        }
        auto odds = new QLabel;
        odds->setMargin(6);
        table->setCellWidget(normal, 3, odds);
        scenarioOdds_ << odds;
        connect(mode, &QComboBox::currentIndexChanged, this, [this] { showScenarioOdds(); });
    }
    auto actions = new QHBoxLayout;
    auto apply = new QPushButton("Apply scenario rates");
    apply->setObjectName("scenarioApply");
    auto restore = new QPushButton("Restore loaded rates");
    restore->setObjectName("scenarioRestore");
    auto inherit = new QPushButton("Inherit both global rates");
    inherit->setObjectName("scenarioInherit");
    actions->addWidget(apply);
    actions->addWidget(restore);
    actions->addWidget(inherit);
    detail->addLayout(actions);
    scenarioNotice_ = new QLabel;
    scenarioNotice_->setObjectName("scenarioNotice");
    scenarioNotice_->setWordWrap(true);
    detail->addWidget(scenarioNotice_);
    auto note = new QLabel(
        "Passing share counts the game's 30-bit random values, including modulo "
        "imbalance. It is not an observed encounter frequency. Zero successful outcomes disables "
        "a roll; matching the possible-outcome count passes this check every time.");
    note->setWordWrap(true);
    detail->addWidget(note);
    detail->addStretch();
    split->addWidget(scenarioDetails_);
    split->setSizes({300, 950});
    connect(scenarioSearch_, &QLineEdit::textChanged, this, [this] { filterScenarios(); });
    connect(scenarios_, &QListWidget::currentRowChanged, this, [this] { showScenario(); });
    connect(apply, &QPushButton::clicked, this, [this] { applyScenario(false); });
    connect(restore, &QPushButton::clicked, this, [this] { applyScenario(true); });
    connect(inherit, &QPushButton::clicked, this, [this] {
        if (!scenarios_->currentItem())
            return;
        auto next = *project_->encounters();
        const int key = scenarios_->currentItem()->data(Qt::UserRole).toInt();
        next.setScenarioBranch(key, false, std::nullopt);
        next.setScenarioBranch(key, true, std::nullopt);
        commit(next, "Inherit global encounter rates");
        showScenario();
    });
    return page;
}
void EncounterEditor::refreshScenarios() {
    if (!scenarios_)
        return;
    const int selected =
        scenarios_->currentItem() ? scenarios_->currentItem()->data(Qt::UserRole).toInt() : 1;
    QSignalBlocker block(scenarios_);
    scenarios_->clear();
    const auto &model = *project_->encounters();
    for (int key = 0; key < ScenarioRates::count; ++key) {
        const auto label = QString("%1 · %2%3")
                               .arg(key)
                               .arg(model.scenarioLabel(key))
                               .arg(model.scenarioRates().branch(key, false) ||
                                            model.scenarioRates().branch(key, true)
                                        ? " [override]"
                                        : "");
        auto item = new QListWidgetItem(label, scenarios_);
        item->setData(Qt::UserRole, key);
    }
    scenarios_->setCurrentRow(selected);
    filterScenarios();
    showScenario();
}
void EncounterEditor::filterScenarios() {
    const auto query = scenarioSearch_->text().trimmed();
    for (int i = 0; i < scenarios_->count(); ++i)
        scenarios_->item(i)->setHidden(
            !query.isEmpty() && !scenarios_->item(i)->text().contains(query, Qt::CaseInsensitive));
    if (!scenarios_->currentItem() || scenarios_->currentItem()->isHidden()) {
        int first = -1;
        for (int i = 0; i < scenarios_->count(); ++i)
            if (!scenarios_->item(i)->isHidden()) {
                first = i;
                break;
            }
        scenarios_->setCurrentRow(first);
    }
}
void EncounterEditor::showScenario() {
    scenarioDetails_->setEnabled(scenarios_->currentItem() != nullptr);
    if (!scenarios_->currentItem())
        return;
    const auto &model = *project_->encounters();
    const int key = scenarios_->currentItem()->data(Qt::UserRole).toInt();
    for (int normal = 0; normal < 2; ++normal) {
        const auto &value = model.scenarioRates().branch(key, normal != 0);
        const auto effective = value.value_or(ScenarioRoll{
            model.globalRate().divisor(), normal ? model.globalRate().normalPassCount()
                                                 : model.globalRate().alternatePassCount()});
        QSignalBlocker modeBlock(scenarioModes_[normal]), divBlock(scenarioDivisors_[normal]),
            passBlock(scenarioPasses_[normal]);
        scenarioModes_[normal]->setCurrentIndex(!value                                         ? 0
                                                : value->divisor == 1 && value->passCount == 0 ? 2
                                                                                               : 1);
        scenarioDivisors_[normal]->setValue(effective.divisor);
        scenarioPasses_[normal]->setValue(effective.passCount);
    }
    scenarioNotice_->clear();
    showScenarioOdds();
}
void EncounterEditor::showScenarioOdds() {
    if (scenarioOdds_.size() != 2)
        return;
    const auto &global = project_->encounters()->globalRate();
    for (int normal = 0; normal < 2; ++normal) {
        const auto mode = scenarioModes_[normal]->currentIndex();
        scenarioDivisors_[normal]->setEnabled(mode == 1);
        scenarioPasses_[normal]->setEnabled(mode == 1);
        const auto divisor = mode == 0   ? global.divisor()
                             : mode == 2 ? 1U
                                         : quint32(scenarioDivisors_[normal]->value());
        const auto pass = mode == 0
                              ? (normal ? global.normalPassCount() : global.alternatePassCount())
                          : mode == 2 ? 0U
                                      : quint32(scenarioPasses_[normal]->value());
        scenarioOdds_[normal]->setText(
            pass > divisor ? "Invalid count"
                           : QString("%1%").arg(100 * EncounterRate::passingFraction(divisor, pass),
                                                0, 'f', 8));
    }
}
void EncounterEditor::applyScenario(bool restore) {
    if (!scenarios_->currentItem())
        return;
    try {
        const auto key = scenarios_->currentItem()->data(Qt::UserRole).toInt();
        auto next = *project_->encounters();
        const auto original = restore ? ScenarioRates::load(project_->rom()) : ScenarioRates{};
        for (int normal = 0; normal < 2; ++normal) {
            const auto mode = scenarioModes_[normal]->currentIndex();
            const auto value =
                restore ? original.branch(key, normal != 0)
                : mode == 0
                    ? std::optional<ScenarioRoll>{}
                    : std::optional<ScenarioRoll>{
                          mode == 2 ? ScenarioRoll{1, 0}
                                    : ScenarioRoll{quint32(scenarioDivisors_[normal]->value()),
                                                   quint32(scenarioPasses_[normal]->value())}};
            next.setScenarioBranch(key, normal != 0, value);
        }
        commit(next,
               restore ? "Restore scenario encounter rates" : "Edit scenario encounter rates");
        showScenario();
    } catch (const std::exception &e) {
        scenarioNotice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
