#include "app/content_refresh.h"
#include "app/view_position.h"
#include "app/scenario_editor.h"
#include "app/game_rules_editor.h"
#include "app/scenario_squad_panel.h"
#include "app/scenario_map_editor.h"
#include "app/scenario_route_editor.h"
#include "app/scenario_event_editor.h"
#include <QCheckBox>
#include <QTimeEdit>
#include <QGroupBox>
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
    auto navigationRow = new QHBoxLayout;
    auto browse = new QPushButton("Scenarios"); browse->setCheckable(true); browse->setChecked(true);
    browse->setObjectName("scenarioBrowse"); navigationRow->addWidget(browse); navigationRow->addWidget(navigation);
    auto title = new QLabel; title->setObjectName("scenarioCurrentTitle"); navigationRow->addWidget(title,1);
    auto test = new QPushButton("Test this mission…"); test->setObjectName("scenarioTestMission");
    navigationRow->addWidget(test);
    connect(test, &QPushButton::clicked, this, [this] { if (key() >= 0 && testScenario) testScenario(key()); });
    layout->addLayout(navigationRow);
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
    connect(browse,&QPushButton::toggled,picker,&QWidget::setVisible);
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
    base_->setKeyboardTracking(false);
    top->addWidget(new QLabel("This mission · Applied automatically"));
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
    addPage(routes_, "Orders");
    auto settingsPage = new QWidget;
    settingsPage->setObjectName("scenarioSettings");
    auto settingsLayout = new QVBoxLayout(settingsPage);
    auto clockBox = new QGroupBox("Mission starting time");
    auto clockForm = new QFormLayout(clockBox);
    missionTime_ = new QTimeEdit;
    missionTime_->setDisplayFormat("HH:mm");
    missionTime_->setKeyboardTracking(false);
    missionTime_->setObjectName("scenarioStartingTime");
    clockForm->addRow("Start at", missionTime_);
    auto timeHelp = new QLabel("Applies when entering this mission from the world map. A suspended mission keeps its saved clock. Daylight runs from 06:00 to 18:00.");
    timeHelp->setWordWrap(true);
    clockForm->addRow(timeHelp);
    settingsLayout->addWidget(clockBox);
    settingsLayout->addWidget(new GameRulesEditor(project_, undo_, changed_, {"clock"}));
    auto rewardBox = new QGroupBox("Completion item reward");
    auto rewardForm = new QFormLayout(rewardBox);
    auto rewardHelp = new QLabel("The completion screen awards the primary boss item. If that item was already collected or is None, it uses the fallback boss item. These choices also update the corresponding squad's drop.");
    rewardHelp->setWordWrap(true);
    rewardForm->addRow(rewardHelp);
    for (int i = 0; i < 2; ++i) {
        missionRewards_[i] = choice(QString("scenarioMissionReward%1").arg(i));
        rewardForm->addRow(i ? "Fallback boss item" : "Primary boss item", missionRewards_[i]);
        connect(missionRewards_[i], &QComboBox::activated, this, [this, i] {
            if (loading_ || key() < 0) return;
            change("Change completion item reward", [this, i](Scenarios &next) {
                const int value = selected(missionRewards_[i]);
                const auto rows = next.scenario(key()).layout.deployments;
                for (int row = 0; row < rows.size(); ++row) {
                    if ((u8(rows[row], 0) & 127) != 30 + i) continue;
                    auto record = rows[row];
                    put8(record, 10, (value & 0x8000) ? 1 : 0);
                    put16(record, 11, quint16(value & 0x7fff));
                    next.setDeployment(key(), row, record, project_->classData());
                    return;
                }
            });
        });
    }
    settingsLayout->addWidget(rewardBox);
    auto townsHelp = new QLabel("Town names and populations: open Map, then select a town marker or its label. Changes apply automatically and affect every scenario that uses that town.");
    townsHelp->setWordWrap(true);
    settingsLayout->addWidget(townsHelp);
    settingsLayout->addStretch();
    auto settingsScroll = new QScrollArea;
    settingsScroll->setWidgetResizable(true);
    settingsScroll->setFrameShape(QFrame::NoFrame);
    settingsLayout->setSizeConstraint(QLayout::SetMinimumSize);
    settingsScroll->setWidget(settingsPage);
    addPage(settingsScroll, "Settings");
    events_ = new ScenarioEventEditor(project_, undo_, changed_);
    addPage(events_, "Events");
    connect(missionTime_, &QTimeEdit::timeChanged, this, [this](QTime time) {
        if (loading_ || key() < 0) return;
        change("Change mission starting time", [this, time](Scenarios &next) {
            next.setStartTime(key(), time.hour(), time.minute(), project_->classData());
        });
    });
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
    auto locate = new QPushButton("Show selected squad on map"); locate->setObjectName("scenarioLocateSquad");
    squad->addWidget(locate);
    connect(locate,&QPushButton::clicked,this,[this,pages]{map_->selectSquad(editingRow_);pages->setCurrentWidget(map_);});
    auto panelScroll = new QScrollArea;panelScroll->setWidgetResizable(true);panelScroll->setFrameShape(QFrame::NoFrame);
    squadPanel_=new ScenarioSquadPanel(project_,[this](const Scenarios &next,const QString &label){commit(next,label);},"scenario");
    panelScroll->setWidget(squadPanel_);squad->addWidget(panelScroll,1);
    body->addWidget(squadDetails_);body->setSizes({220,700});
    notice_ = new QLabel;
    notice_->setObjectName("scenarioEditNotice");
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details_);
    split->setSizes({210, 1220});
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(scenarios_, &QListWidget::currentRowChanged, this, [this] { showScenario(); });
    connect(squads_, &QListWidget::currentRowChanged, this, [this] {
        if(loading_)return;showSquad();map_->selectSquad(editingRow_);routes_->selectSquad(editingRow_);
    });
    map_->squadSelected=[this](int row){
        if(loading_)return;const QSignalBlocker block(squads_);squads_->setCurrentRow(row);showSquad();routes_->selectSquad(row);
    };
    routes_->squadSelected=[this](int row){
        if(loading_)return;const QSignalBlocker block(squads_);squads_->setCurrentRow(row);showSquad();map_->selectSquad(row);
    };
    connect(base_, &QSpinBox::valueChanged, this, [this] {
        if(loading_ || key()<0) return;
        change("Change scenario base level", [this](Scenarios &next) {next.setBaseLevel(key(),base_->value(),project_->classData());});
        const QSignalBlocker block(base_);base_->setValue(project_->scenarios()->scenario(key()).layout.baseLevel());
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        change("Restore loaded scenario", [this](Scenarios &next) { next.restore(key()); });
    });
    connect(create, &QPushButton::clicked, this, [this] {
        change("New scenario squad", [this](Scenarios &next) {
            editingRow_ = next.addSquad(key(), -1, project_->classData());
        });
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
    watchContentChanges(undo_, this, [this] {
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
    for(auto box:missionRewards_) {
        const QSignalBlocker blocker(box);box->clear();
        for(const auto kind:{Table::Consumables,Table::Items})for(const auto &r:project_->records(kind))
            box->addItem(QString("%1 %2 · %3").arg(kind==Table::Items?"Equipment":"Consumable").arg(r.id).arg(r.name.simplified()),r.id|(kind==Table::Items?0x8000:0));
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
        events_->selectScenario(-1);
        return;
    }
    QSignalBlocker block(squads_);
    const auto &s = project_->scenarios()->scenario(key());
    if (editingKey_ != key())
        editingRow_ = 0;
    editingKey_ = key();
    { const QSignalBlocker baseBlock(base_);base_->setValue(s.layout.baseLevel()); }
    findChild<QLabel *>("scenarioCurrentTitle")->setText(s.name);
    {
        const QSignalBlocker blocker(missionTime_);
        missionTime_->setTime(QTime(s.layout.startHour(), s.layout.startMinute()));
    }
    for (int i = 0; i < 2; ++i) {
        auto box = missionRewards_[i];
        const QSignalBlocker blocker(box);
        bool found = false;
        for (const auto &row : s.layout.deployments) {
            if ((u8(row, 0) & 127) != 30 + i) continue;
            selectValue(box, u16(row, 11) | (u8(row, 10) ? 0x8000 : 0));
            found = true;
            break;
        }
        if (!found) box->setCurrentIndex(-1);
        box->setEnabled(found);
        box->setToolTip(found ? "Shared with this boss squad's drop" : "This scenario has no corresponding boss squad");
    }
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
    events_->selectScenario(key());
}
void ScenarioEditor::showSquad() {
    if(loading_) return;
    editingRow_=squads_->currentRow();squadDetails_->setEnabled(editingRow_>=0);
    squadPanel_->refresh(key(),editingRow_);
    if(editingRow_>=0) {
        const auto &d=project_->scenarios()->scenario(key()).layout.deployments[editingRow_];
        bool original=false;for(const auto &r:project_->scenarios()->original(key()).layout.deployments)original|=(u8(r,0)&127)==(u8(d,0)&127);
        remove_->setEnabled(!original);
    }
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
