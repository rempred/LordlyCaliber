#include "app/scenario_editor.h"
#include "app/scenario_map_editor.h"
#include "app/scenario_route_editor.h"
#include "app/scenario_world_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>

namespace lordly {
class ScenarioCommand final : public QUndoCommand {
  public:
    ScenarioCommand(Project *project, const QString &label, QJsonObject before, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override {
        project_->setScenarios(before_);
    }
    void redo() override {
        project_->setScenarios(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
static QComboBox *choice(const QString &name) {
    auto box = new QComboBox;
    box->setObjectName(name);
    box->setEditable(true);
    box->setInsertPolicy(QComboBox::NoInsert);
    box->completer()->setCompletionMode(QCompleter::PopupCompletion);
    box->completer()->setFilterMode(Qt::MatchContains);
    box->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    return box;
}
static void selectValue(QComboBox *box, int value) {
    auto index = box->findData(value);
    if (index < 0) {
        box->addItem(QString("Original value %1").arg(value), value);
        index = box->count() - 1;
    }
    box->setCurrentIndex(index);
}
static int selected(QComboBox *box) {
    if (box->currentIndex() < 0 || box->currentText() != box->itemText(box->currentIndex()))
        throw Error("Choose a listed value or finish the search before applying");
    return box->currentData().toInt();
}
ScenarioEditor::ScenarioEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                               QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("scenarioEditor");
    auto layout = new QVBoxLayout(this);
    search_ = new QLineEdit;
    search_->setObjectName("scenarioSearch");
    search_->setPlaceholderText("Find a scenario by name or key…");
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);
    auto split = new QSplitter;
    layout->addWidget(split, 1);
    scenarios_ = new QListWidget;
    scenarios_->setObjectName("scenarioList");
    scenarios_->setMinimumWidth(190);
    split->addWidget(scenarios_);
    details_ = new QWidget;
    auto detail = new QVBoxLayout(details_);
    auto top = new QHBoxLayout;
    top->addWidget(new QLabel("Base level"));
    base_ = new QSpinBox;
    base_->setObjectName("scenarioBase");
    base_->setRange(1, 99);
    top->addWidget(base_);
    auto baseApply = new QPushButton("Apply base level");
    baseApply->setObjectName("scenarioBaseApply");
    top->addWidget(baseApply);
    auto restore = new QPushButton("Restore loaded scenario");
    restore->setObjectName("scenarioRestoreLoaded");
    top->addWidget(restore);
    top->addStretch();
    detail->addLayout(top);
    auto body = new QSplitter;
    auto pages = new QTabWidget;
    pages->setObjectName("scenarioPages");
    detail->addWidget(pages, 1);
    pages->addTab(body, "Squads");
    routes_ = new ScenarioRouteEditor(
        project_, [this](const Scenarios &next, const QString &label) { commit(next, label); });
    pages->addTab(routes_, "Routes and conditions");
    world_ = new ScenarioWorldEditor(project_, undo_, changed_);
    pages->addTab(world_, "Sites and treasure");
    map_ = new ScenarioMapEditor(
        project_, undo_,
        [this](const Scenarios &next, const QString &label) { commit(next, label); },
        [this, pages](MapObject kind, int row) {
            if (kind == MapObject::Squad) {
                squads_->setCurrentRow(row);
                pages->setCurrentIndex(0);
            } else if (kind == MapObject::Waypoint) {
                routes_->selectNode(row);
                pages->setCurrentIndex(1);
            } else {
                world_->selectObject(kind == MapObject::Treasure, row);
                pages->setCurrentIndex(2);
            }
        });
    pages->addTab(map_, "Map");
    auto roster = new QWidget;
    auto rosterLayout = new QVBoxLayout(roster);
    rosterLayout->setContentsMargins(0, 0, 0, 0);
    squads_ = new QListWidget;
    squads_->setObjectName("scenarioSquads");
    rosterLayout->addWidget(squads_);
    auto add = new QPushButton("Duplicate as new squad");
    add->setObjectName("scenarioAddSquad");
    rosterLayout->addWidget(add);
    remove_ = new QPushButton("Remove added squad");
    remove_->setObjectName("scenarioRemoveSquad");
    rosterLayout->addWidget(remove_);
    body->addWidget(roster);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    squadDetails_ = new QWidget;
    auto squad = new QVBoxLayout(squadDetails_);
    auto explanation = new QLabel("Each cohort shares a class, level offset and equipment. "
                                  "A is the leader; B and C can each place up to three members. "
                                  "Apply changes to this deployment only.");
    explanation->setWordWrap(true);
    squad->addWidget(explanation);
    cohorts_ = new QTableWidget(3, 7);
    cohorts_->setObjectName("scenarioCohorts");
    cohorts_->setHorizontalHeaderLabels({"Class", "Level ±", "Equipment 1", "Equipment 2",
                                         "Position 1", "Position 2", "Position 3"});
    cohorts_->setVerticalHeaderLabels({"A · Leader", "B", "C"});
    cohorts_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    cohorts_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    cohorts_->setMinimumHeight(160);
    cohorts_->setMaximumHeight(180);
    squad->addWidget(cohorts_);
    for (int cohort = 0; cohort < 3; ++cohort) {
        cohorts_->setRowHeight(cohort, 36);
        auto cls = choice(QString("scenarioClass_%1").arg(cohort));
        classes_ << cls;
        cohorts_->setCellWidget(cohort, 0, cls);
        auto level = new QSpinBox;
        level->setObjectName(QString("scenarioLevel_%1").arg(cohort));
        level->setRange(-128, 127);
        levels_ << level;
        cohorts_->setCellWidget(cohort, 1, level);
        for (int e = 0; e < 2; ++e) {
            auto box = choice(QString("scenarioGear_%1_%2").arg(cohort).arg(e));
            for (const auto &item : project_->records(Table::Items))
                box->addItem(QString("%1 · %2").arg(item.id).arg(item.name), item.id);
            gear_ << box;
            cohorts_->setCellWidget(cohort, e + 2, box);
        }
        for (int p = 0; p < 3; ++p) {
            auto box = new QComboBox;
            box->setObjectName(QString("scenarioPosition_%1_%2").arg(cohort).arg(p));
            if (cohort || p)
                box->addItem("Absent", 0);
            for (int cell = 1; cell <= 9; ++cell)
                box->addItem(QString::number(cell), cell);
            positions_ << box;
            box->setEnabled(cohort || !p);
            cohorts_->setCellWidget(cohort, p + 4, box);
            connect(box, &QComboBox::currentIndexChanged, this, [this] { preview(); });
        }
        connect(cls, &QComboBox::currentIndexChanged, this, [this] { preview(); });
        connect(level, &QSpinBox::valueChanged, this, [this] { preview(); });
    }
    auto formationRow = new QHBoxLayout;
    auto grid = new QGridLayout;
    for (int i = 0; i < 9; ++i) {
        const int cell = 9 - i;
        auto button = new QPushButton(QString::number(cell));
        button->setObjectName(QString("scenarioCell_%1").arg(cell));
        button->setMinimumSize(75, 48);
        cells_ << button;
        grid->addWidget(button, i / 3, i % 3);
        connect(button, &QPushButton::clicked, this, [this, cell] {
            const int cohort = std::max(0, cohorts_->currentRow());
            const int position = cohort ? std::clamp(cohorts_->currentColumn() - 4, 0, 2) : 0;
            selectValue(positions_[cohort * 3 + position], cell);
        });
    }
    formationRow->addLayout(grid);
    formation_ = new QLabel;
    formation_->setWordWrap(true);
    formationRow->addWidget(formation_, 1);
    squad->addLayout(formationRow);
    auto form = new QFormLayout;
    allied_ = new QCheckBox("Allied deployment");
    allied_->setObjectName("scenarioAllied");
    form->addRow("Side", allied_);
    placement_ = new QComboBox;
    placement_->setObjectName("scenarioPlacement");
    placement_->addItems({"Scenario placement point", "Map coordinates"});
    form->addRow("Placement", placement_);
    auto coordinates = new QHBoxLayout;
    x_ = new QSpinBox;
    x_->setObjectName("scenarioX");
    x_->setRange(0, 255);
    z_ = new QSpinBox;
    z_->setObjectName("scenarioZ");
    z_->setRange(1, 255);
    coordinates->addWidget(new QLabel("Point / X"));
    coordinates->addWidget(x_);
    coordinates->addWidget(new QLabel("Z"));
    coordinates->addWidget(z_);
    form->addRow("Position", coordinates);
    phase_ = new QSpinBox;
    phase_->setObjectName("scenarioPhase");
    phase_->setRange(0, 255);
    form->addRow("Facing phase", phase_);
    start_ = new QComboBox;
    start_->setObjectName("scenarioStart");
    form->addRow("Initial order", start_);
    healing_ = new QComboBox;
    healing_->setObjectName("scenarioHealing");
    const QStringList heals{"None",          "Heal Leaf",
                            "2 Heal Leaves", "2 Heal Leaves + Heal Seed",
                            "2 Heal Seeds",  "2 Heal Seeds + Heal Pack",
                            "2 Heal Packs"};
    for (int i = 0; i < heals.size(); ++i)
        healing_->addItem(heals[i], i);
    form->addRow("Healing supplies", healing_);
    fruit_ = new QComboBox;
    fruit_->setObjectName("scenarioFruit");
    const QStringList fruits{"None", "2 Power Fruits", "Power Fruit + Angel Fruit",
                             "2 Angel Fruits"};
    for (int i = 0; i < fruits.size(); ++i)
        fruit_->addItem(fruits[i], i);
    form->addRow("Fruit supplies", fruit_);
    revival_ = new QComboBox;
    revival_->setObjectName("scenarioRevival");
    for (int i = 0; i < 3; ++i)
        revival_->addItem(i ? QString("%1 Revive Stone(s)").arg(i) : "None", i);
    form->addRow("Revival supplies", revival_);
    drop_ = choice("scenarioDrop");
    for (const auto table : {Table::Consumables, Table::Items})
        for (const auto &r : project_->records(table))
            drop_->addItem(QString("%1 · %2")
                               .arg(table == Table::Items ? "Equipment" : "Consumable")
                               .arg(r.name),
                           r.id | (table == Table::Items ? 0x8000 : 0));
    form->addRow("Defeat drop", drop_);
    squad->addLayout(form);
    placementHelp_ =
        new QLabel("Coordinates use 1/256 of the map's bounds. A zero Z byte "
                   "selects a scenario placement point instead. Facing phase 1–8 "
                   "uses the game's eight directions; other original values are preserved.");
    placementHelp_->setWordWrap(true);
    squad->addWidget(placementHelp_);
    auto apply = new QPushButton("Apply squad changes");
    apply->setObjectName("scenarioSquadApply");
    squad->addWidget(apply);
    squad->addStretch();
    scroll->setWidget(squadDetails_);
    body->addWidget(scroll);
    body->setSizes({220, 960});
    notice_ = new QLabel;
    notice_->setObjectName("scenarioEditNotice");
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details_);
    split->setSizes({230, 1200});
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(scenarios_, &QListWidget::currentRowChanged, this, [this] { showScenario(); });
    connect(squads_, &QListWidget::currentRowChanged, this, [this] { showSquad(); });
    connect(placement_, &QComboBox::currentIndexChanged, this,
            [this] { z_->setEnabled(placement_->currentIndex() == 1); });
    connect(apply, &QPushButton::clicked, this, [this] { applySquad(); });
    connect(baseApply, &QPushButton::clicked, this, [this] {
        change("Change scenario base level", [this](Scenarios &next) {
            next.setBaseLevel(key(), base_->value(), project_->classData());
        });
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        change("Restore loaded scenario", [this](Scenarios &next) { next.restore(key()); });
    });
    connect(add, &QPushButton::clicked, this, [this] {
        change("Add scenario squad", [this](Scenarios &next) {
            editingRow_ = next.addSquad(key(), squads_->currentRow(), project_->classData());
        });
    });
    connect(remove_, &QPushButton::clicked, this, [this] {
        change("Remove scenario squad", [this](Scenarios &next) {
            next.removeSquad(key(), squads_->currentRow(), project_->classData());
        });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    refresh();
}
int ScenarioEditor::key() const {
    return scenarios_->currentItem() ? scenarios_->currentItem()->data(Qt::UserRole).toInt() : -1;
}
void ScenarioEditor::refresh() {
    loading_ = true;
    QSignalBlocker block(scenarios_);
    const int previous = key();
    scenarios_->clear();
    for (const auto &s : project_->scenarios()->entries()) {
        auto item = new QListWidgetItem(
            QString("%1 · %2%3")
                .arg(s.key)
                .arg(s.name)
                .arg(s.json() != project_->scenarios()->original(s.key).json() ? " *" : ""),
            scenarios_);
        item->setData(Qt::UserRole, s.key);
        if (s.key == previous)
            scenarios_->setCurrentItem(item);
    }
    for (auto box : gear_) {
        const QSignalBlocker blocker(box);
        box->clear();
        for (const auto &r : project_->records(Table::Items))
            box->addItem(QString("%1 · %2").arg(r.id).arg(r.name), r.id);
    }
    {
        const QSignalBlocker blocker(drop_);
        drop_->clear();
        for (const auto kind : {Table::Consumables, Table::Items})
            for (const auto &r : project_->records(kind))
                drop_->addItem(QString("%1 %2 · %3")
                                   .arg(kind == Table::Items ? "Equipment" : "Consumable")
                                   .arg(r.id)
                                   .arg(r.name),
                               r.id | (kind == Table::Items ? 0x8000 : 0));
    }
    for (auto box : classes_) {
        box->clear();
        for (const auto &r : project_->records(Table::Classes))
            box->addItem(QString("%1 · %2").arg(r.id).arg(QString(r.name).replace('\n', ' ')),
                         r.id);
    }
    loading_ = false;
    filter();
    showScenario();
}
void ScenarioEditor::filter() {
    int first = -1;
    for (int i = 0; i < scenarios_->count(); ++i) {
        auto item = scenarios_->item(i);
        item->setHidden(!item->text().contains(search_->text().trimmed(), Qt::CaseInsensitive));
        if (!item->isHidden() && first < 0)
            first = i;
    }
    if (!scenarios_->currentItem() || scenarios_->currentItem()->isHidden())
        scenarios_->setCurrentRow(first);
}
void ScenarioEditor::showScenario() {
    if (loading_)
        return;
    const bool valid = key() >= 0;
    details_->setEnabled(valid);
    if (!valid) {
        editingKey_ = -1;
        editingRow_ = -1;
        routes_->refresh(-1);
        world_->refresh(-1);
        return;
    }
    QSignalBlocker block(squads_);
    const auto &s = project_->scenarios()->scenario(key());
    if (editingKey_ != key())
        editingRow_ = 0;
    editingKey_ = key();
    base_->setValue(s.layout.baseLevel());
    squads_->clear();
    for (int i = 0; i < s.squads.size(); ++i) {
        const int id = u8(s.squads[i], 0);
        const auto name = id < project_->records(Table::Classes).size()
                              ? project_->record(Table::Classes, id).name
                              : "Unknown class";
        squads_->addItem(QString("%1 · %2\n%3 members%4")
                             .arg(u8(s.layout.deployments[i], 0) & 127)
                             .arg(QString(name).replace('\n', ' '))
                             .arg(Scenarios::memberCount(s.squads[i]))
                             .arg(u8(s.layout.deployments[i], 0) & 128 ? " · Allied" : ""));
    }
    squads_->setCurrentRow(
        s.squads.isEmpty() ? -1 : std::clamp(editingRow_, 0, int(s.squads.size()) - 1));
    showSquad();
    routes_->refresh(key());
    world_->refresh(key());
    map_->refresh(key());
}
void ScenarioEditor::showSquad() {
    if (loading_)
        return;
    editingRow_ = squads_->currentRow();
    squadDetails_->setEnabled(editingRow_ >= 0);
    if (editingRow_ < 0)
        return;
    loading_ = true;
    const auto &s = project_->scenarios()->scenario(key());
    const auto &r = s.squads[editingRow_], &d = s.layout.deployments[editingRow_];
    const std::array<int, 3> offsets{0, 7, 16};
    for (int c = 0; c < 3; ++c) {
        const int at = offsets[c];
        selectValue(classes_[c], u8(r, at));
        const int level = u8(r, at + 1);
        levels_[c]->setValue(level >= 128 ? level - 256 : level);
        levels_[c]->setEnabled(!(key() == 25 && (u8(d, 0) & 127) == 40 && c));
        for (int e = 0; e < 2; ++e)
            selectValue(gear_[c * 2 + e], u16(r, at + 2 + e * 2));
        for (int p = 0; p < 3; ++p)
            selectValue(positions_[c * 3 + p], c || !p ? u8(r, at + 6 + p) : 0);
    }
    allied_->setChecked(u8(d, 0) & 128);
    placement_->setCurrentIndex(u8(d, 4) ? 1 : 0);
    x_->setValue(u8(d, 3));
    z_->setValue(std::max(1, int(u8(d, 4))));
    z_->setEnabled(u8(d, 4));
    phase_->setValue(u8(d, 5));
    start_->clear();
    start_->addItem("1 · Hold position", 1);
    for (const auto &n : s.layout.nodes)
        start_->addItem(QString("Node %1").arg(u8(n, 0)), u8(n, 0));
    selectValue(start_, u8(d, 6));
    selectValue(healing_, u8(d, 13));
    selectValue(fruit_, u8(d, 14));
    selectValue(revival_, u8(d, 15));
    selectValue(drop_, u16(d, 11) | (u8(d, 10) ? 0x8000 : 0));
    bool original = false;
    for (const auto &row : project_->scenarios()->original(key()).layout.deployments)
        original |= (u8(row, 0) & 127) == (u8(d, 0) & 127);
    remove_->setEnabled(!original);
    notice_->clear();
    loading_ = false;
    preview();
}
void ScenarioEditor::preview() {
    if (loading_ || key() < 0)
        return;
    QStringList info;
    int count = 0;
    for (int i = 0; i < 9; ++i) {
        QStringList names;
        for (int c = 0; c < 3; ++c)
            for (int p = 0; p < (c ? 3 : 1); ++p)
                if (positions_[c * 3 + p]->currentData().toInt() == 9 - i) {
                    names << QString(QChar('A' + c));
                    ++count;
                }
        cells_[i]->setText(QString("%1\n%2").arg(9 - i).arg(names.join(" + ")));
        cells_[i]->setStyleSheet(names.size() > 1 ? "background:#a53b35;color:white" : "");
    }
    for (int c = 0; c < 3; ++c)
        info << QString("%1: level %2")
                    .arg(QChar('A' + c))
                    .arg(project_->scenarios()->scenario(key()).layout.baseLevel() +
                         levels_[c]->value());
    formation_->setText(QString("Front row above · rear row below\n%1 members\n%2\nSelect a cohort "
                                "position, then click a cell.")
                            .arg(count)
                            .arg(info.join(" · ")));
}
void ScenarioEditor::applySquad() {
    if (key() < 0 || editingRow_ < 0)
        return;
    change("Edit scenario squad", [this](Scenarios &next) {
        auto r = next.scenario(key()).squads[editingRow_];
        auto d = next.scenario(key()).layout.deployments[editingRow_];
        const std::array<int, 3> offsets{0, 7, 16};
        for (int c = 0; c < 3; ++c) {
            const int at = offsets[c];
            put8(r, at, quint8(selected(classes_[c])));
            put8(r, at + 1, quint8(levels_[c]->value()));
            for (int e = 0; e < 2; ++e)
                put16(r, at + 2 + e * 2, quint16(selected(gear_[c * 2 + e])));
            for (int p = 0; p < (c ? 3 : 1); ++p)
                put8(r, at + 6 + p, quint8(selected(positions_[c * 3 + p])));
        }
        put8(d, 0, quint8((u8(d, 0) & 127) | (allied_->isChecked() ? 128 : 0)));
        put8(d, 3, quint8(x_->value()));
        put8(d, 4, quint8(placement_->currentIndex() ? z_->value() : 0));
        put8(d, 5, quint8(phase_->value()));
        put8(d, 6, quint8(selected(start_)));
        const int drop = selected(drop_);
        if (drop != (u16(d, 11) | (u8(d, 10) ? 0x8000 : 0))) {
            put8(d, 10, (drop & 0x8000) ? 1 : 0);
            put16(d, 11, quint16(drop & 0x7fff));
        }
        put8(d, 13, quint8(selected(healing_)));
        put8(d, 14, quint8(selected(fruit_)));
        put8(d, 15, quint8(selected(revival_)));
        next.editSquad(key(), editingRow_, r, d, project_->classData());
    });
}
void ScenarioEditor::commit(const Scenarios &next, const QString &label) {
    const auto before = project_->scenarios()->json(), after = next.json();
    if (before != after)
        undo_->push(new ScenarioCommand(project_, label, before, after));
}
void ScenarioEditor::change(const QString &label, const std::function<void(Scenarios &)> &edit) {
    try {
        auto next = *project_->scenarios();
        edit(next);
        commit(next, label);
        notice_->setText("Changes applied.");
    } catch (const Error &error) {
        notice_->setText(QString::fromUtf8(error.what()));
    }
}
} // namespace lordly
