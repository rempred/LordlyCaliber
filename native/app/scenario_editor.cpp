#include "app/view_position.h"
#include "app/scenario_editor.h"
#include "app/squad_members_editor.h"
#include "app/scenario_map_editor.h"
#include "app/scenario_route_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto navigation = new QTabBar;
    navigation->setObjectName("scenarioNavigation");
    navigation->setExpanding(false);
    navigation->setDrawBase(false);
    layout->addWidget(navigation);
    auto content = new QWidget;
    auto contentLayout = new QVBoxLayout(content);
    layout->addWidget(content, 1);
    auto split = new QSplitter;
    contentLayout->addWidget(split);
    auto picker = new QWidget;
    auto pickerLayout = new QVBoxLayout(picker);
    pickerLayout->setContentsMargins(0, 0, 0, 0);
    search_ = new QLineEdit;
    search_->setObjectName("scenarioSearch");
    search_->setPlaceholderText("Find a scenario…");
    search_->setToolTip("Search by map, title, branch, or runtime key");
    search_->setClearButtonEnabled(true);
    pickerLayout->addWidget(search_);
    scenarios_ = new QListWidget;
    scenarios_->setObjectName("scenarioList");
    scenarios_->setMinimumWidth(190);
    pickerLayout->addWidget(scenarios_, 1);
    split->addWidget(picker);
    details_ = new QWidget;
    auto detail = new QVBoxLayout(details_);
    detail->setContentsMargins(0, 0, 0, 0);
    auto pages = new QStackedWidget;
    pages->setObjectName("scenarioPages");
    detail->addWidget(pages, 1);
    connect(navigation, &QTabBar::currentChanged, pages, &QStackedWidget::setCurrentIndex);
    connect(pages, &QStackedWidget::currentChanged, navigation, &QTabBar::setCurrentIndex);
    auto addPage = [&](QWidget *page, const QString &title) {
        pages->addWidget(page);
        navigation->addTab(title);
    };
    auto squadPage = new QWidget;
    auto squadLayout = new QVBoxLayout(squadPage);
    squadLayout->setContentsMargins(0, 0, 0, 0);
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
    squadLayout->addLayout(top);
    auto body = new QSplitter;
    squadLayout->addWidget(body, 1);
    routes_ = new ScenarioRouteEditor(
        project_, [this](const Scenarios &next, const QString &label) { commit(next, label); });
    map_ = new ScenarioMapEditor(
        project_, undo_,
        [this](const Scenarios &next, const QString &label) { commit(next, label); },
        [this, pages, squadPage](MapObject kind, int row) {
            if (kind == MapObject::Squad) {
                squads_->setCurrentRow(row);
                pages->setCurrentWidget(squadPage);
            } else if (kind == MapObject::Waypoint) {
                routes_->selectNode(row);
                pages->setCurrentWidget(routes_);
            }
        });
    addPage(map_, "Map");
    addPage(squadPage, "Squads");
    addPage(routes_, "Routes and conditions");
    auto roster = new QWidget;
    auto rosterLayout = new QVBoxLayout(roster);
    rosterLayout->setContentsMargins(0, 0, 0, 0);
    squads_ = new QListWidget;
    squads_->setObjectName("scenarioSquads");
    rosterLayout->addWidget(squads_);
    auto create = new QPushButton("New squad");
    create->setObjectName("scenarioNewSquad");
    rosterLayout->addWidget(create);
    auto add = new QPushButton("Duplicate");
    add->setObjectName("scenarioAddSquad");
    rosterLayout->addWidget(add);
    remove_ = new QPushButton("Remove added squad");
    remove_->setObjectName("scenarioRemoveSquad");
    rosterLayout->addWidget(remove_);
    body->addWidget(roster);
    squadDetails_ = new QWidget;
    auto squad = new QVBoxLayout(squadDetails_);
    auto squadTabs = new QTabWidget;
    squadTabs->setObjectName("scenarioSquadTabs");
    squad->addWidget(squadTabs, 1);
    auto memberPage = new QWidget;
    auto memberLayout = new QVBoxLayout(memberPage);
    memberEditor_ = new SquadMembersEditor(project_, "scenario", 1, 1);
    memberLayout->addWidget(memberEditor_);
    auto levelHelp = new QLabel("Level offsets are added to the scenario base level. Each member must be level 1–99.");
    levelHelp->setWordWrap(true);
    memberLayout->addWidget(levelHelp);
    memberLayout->addStretch();
    auto memberScroll = new QScrollArea;
    memberScroll->setWidgetResizable(true);
    memberScroll->setFrameShape(QFrame::NoFrame);
    memberLayout->setSizeConstraint(QLayout::SetMinimumSize);
    memberScroll->setWidget(memberPage);
    squadTabs->addTab(memberScroll, "Members and formation");
    auto deploymentPage = new QWidget;
    auto deploymentLayout = new QVBoxLayout(deploymentPage);
    auto deploymentScroll = new QScrollArea;
    deploymentScroll->setWidgetResizable(true);
    deploymentScroll->setFrameShape(QFrame::NoFrame);
    deploymentLayout->setSizeConstraint(QLayout::SetMinimumSize);
    deploymentScroll->setWidget(deploymentPage);
    squadTabs->addTab(deploymentScroll, "Deployment and supplies");
    connect(memberEditor_, &SquadMembersEditor::edited, this, [this] { preview(); });
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
    deploymentLayout->addLayout(form);
    placementHelp_ =
        new QLabel("Coordinates use 1/256 of the map's bounds. A zero Z byte "
                   "selects a scenario placement point instead. Facing phase 1–8 "
                   "uses the game's eight directions; other original values are preserved.");
    placementHelp_->setWordWrap(true);
    deploymentLayout->addWidget(placementHelp_);
    deploymentLayout->addStretch();
    auto restoreSquad = new QPushButton("Restore loaded squad");
    restoreSquad->setObjectName("scenarioRestoreSquad");
    auto apply = new QPushButton("Apply squad");
    apply->setObjectName("scenarioSquadApply");
    auto squadActions = new QHBoxLayout;
    squadActions->addWidget(apply);
    squadActions->addWidget(restoreSquad);
    squadActions->addStretch();
    squad->addLayout(squadActions);
    body->addWidget(squadDetails_);
    body->setSizes({220, 960});
    notice_ = new QLabel;
    notice_->setObjectName("scenarioEditNotice");
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details_);
    split->setSizes({300, 1130});
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
    connect(create, &QPushButton::clicked, this, [this] {
        change("New scenario squad", [this](Scenarios &next) {
            editingRow_ = next.addSquad(key(), -1, project_->classData());
        });
    });
    connect(restoreSquad, &QPushButton::clicked, this, [this] {
        bool restored = false;
        change("Restore loaded scenario squad", [this, &restored](Scenarios &next) {
            if (editingRow_ < 0) throw Error("Choose a squad first");
            auto d = next.scenario(key()).layout.deployments[editingRow_];
            const auto &base = next.original(key());
            for (int row = 0; row < base.squads.size(); ++row) {
                if ((u8(d,0)&127) != (u8(base.layout.deployments[row],0)&127)) continue;
                const auto identity = d.mid(1,2), routeTail = d.right(2);
                d = base.layout.deployments[row];
                d.replace(1,2,identity); d.replace(16,2,routeTail);
                next.editSquad(key(), editingRow_, base.squads[row], d, project_->classData());
                restored = true;
                return;
            }
            throw Error("This squad was added to the project and has no loaded version");
        });
        if (restored) {
            showSquad();
            notice_->setText("Loaded squad restored.");
        }
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
    const auto item = scenarios_->currentItem();
    return item && (item->flags() & Qt::ItemIsEnabled) ? item->data(Qt::UserRole).toInt() : -1;
}
void ScenarioEditor::refresh() {
    const ViewPosition keepView(this);
    loading_ = true;
    QSignalBlocker block(scenarios_);
    const int previous = key();
    QFile labels(":/game/encounters/labels.json");
    QJsonObject catalog;
    if (labels.open(QIODevice::ReadOnly))
        catalog = QJsonDocument::fromJson(labels.readAll()).object();
    const auto notes = catalog["notes"].toObject();
    const auto names = catalog["labels"].toObject();
    const auto &entries = project_->scenarios()->entries();
    auto keys = entries.keys();
    QSet<int> devKeys;
    for (const auto value : catalog["devKeys"].toArray()) {
        const int id = value.toInt();
        devKeys.insert(id);
        if (!entries.contains(id))
            keys.append(id);
    }
    std::sort(keys.begin(), keys.end(), [&](int a, int b) {
        if (devKeys.contains(a) != devKeys.contains(b))
            return !devKeys.contains(a);
        return a < b;
    });
    scenarios_->clear();
    for (const int id : keys) {
        const auto entry = entries.constFind(id);
        const bool editable = entry != entries.cend();
        const auto name = editable ? entry->name : names[QString::number(id)].toString();
        const bool modified = editable && entry->json() != project_->scenarios()->original(id).json();
        auto item = new QListWidgetItem(
            QString("%1 · %2%3%4").arg(id).arg(name)
                .arg(editable ? "" : " — editing unavailable").arg(modified ? " *" : ""),
            scenarios_);
        item->setData(Qt::UserRole, id);
        if (!editable)
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        auto help = QString("Runtime key %1\n%2").arg(id).arg(name);
        const auto note = notes[QString::number(id)].toString();
        if (!note.isEmpty())
            help += "\n\n" + note;
        item->setToolTip(help);
        if (editable && id == previous)
            scenarios_->setCurrentItem(item);
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
    loading_ = false;
    filter();
    showScenario();
}
void ScenarioEditor::filter() {
    int first = -1;
    for (int i = 0; i < scenarios_->count(); ++i) {
        auto item = scenarios_->item(i);
        item->setHidden(!item->text().contains(search_->text().trimmed(), Qt::CaseInsensitive));
        if (!item->isHidden() && (item->flags() & Qt::ItemIsEnabled) && first < 0)
            first = i;
    }
    if (key() < 0 || scenarios_->currentItem()->isHidden())
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
        map_->refresh(-1);
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
    const auto members = squadMembers(r);
    if (members.size() > 5) {
        squadDetails_->setEnabled(false);
        loading_ = false;
        notice_->setText("This loaded squad has more than five members. Its original data is preserved; "
                         "the member editor supports up to five.");
        return;
    }
    memberEditor_->setMembers(members);
    if (key() == 25 && (u8(d, 0) & 127) == 40)
        for (int i = 1; i < members.size(); ++i) memberEditor_->setLevelEditable(i, false);
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
}
void ScenarioEditor::preview() {
    if (!loading_ && key() >= 0) notice_->setText("Unapplied changes — Apply squad updates the project.");
}
void ScenarioEditor::applySquad() {
    if (key() < 0 || editingRow_ < 0)
        return;
    change("Edit scenario squad", [this](Scenarios &next) {
        auto r = next.scenario(key()).squads[editingRow_];
        auto d = next.scenario(key()).layout.deployments[editingRow_];
        r = editSquadMembers(r, memberEditor_->members());
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
