#include "app/custom_squad_editor.h"
#include "app/squad_members_editor.h"
#include "app/encounter_labels.h"
#include "app/view_position.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QStandardItemModel>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
static QComboBox *choice(const QString &name, bool searchable = true) {
    auto box = new QComboBox;
    box->setObjectName(name);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(10);
    if (searchable) {
        box->setEditable(true);
        box->setInsertPolicy(QComboBox::NoInsert);
        box->completer()->setCompletionMode(QCompleter::PopupCompletion);
        box->completer()->setFilterMode(Qt::MatchContains);
        box->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    }
    return box;
}
static int selected(QComboBox *box) {
    const auto index = box->currentIndex();
    if (index < 0 || box->currentText() != box->itemText(index))
        throw Error("Choose a matching entry from each search list");
    return box->currentData().toInt();
}
static QSpinBox *spin(const QString &name, int minimum, int maximum) {
    auto box = new QSpinBox;
    box->setObjectName(name);
    box->setRange(minimum, maximum);
    return box;
}
CustomSquadEditor::CustomSquadEditor(
    Project *project, QUndoStack *undo,
    std::function<void(const Encounters &, const QString &)> commit, QWidget *parent)
    : QWidget(parent), project_(project), commit_(std::move(commit)) {
    setObjectName("customSquadsPage");
    project_->loadEncounters();
    auto layout = new QVBoxLayout(this);
    auto intro = new QLabel(
        "Replace both creature choices for one scenario and terrain slot with a squad. "
        "The scenario and terrain encounter rolls still apply. The first member is the leader.");
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto split = new QSplitter;
    layout->addWidget(split, 1);
    auto browser = new QWidget;
    auto left = new QVBoxLayout(browser);
    left->setContentsMargins(0, 0, 8, 0);
    search_ = new QLineEdit;
    search_->setObjectName("customSearch");
    search_->setPlaceholderText("Find a squad, scenario or key…");
    search_->setClearButtonEnabled(true);
    left->addWidget(search_);
    list_ = new QListWidget;
    list_->setObjectName("customSquadList");
    list_->setWordWrap(true);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left->addWidget(list_, 1);
    auto create = new QPushButton("New squad");
    create->setObjectName("customNew");
    copy_ = new QPushButton("Duplicate");
    copy_->setObjectName("customCopy");
    auto actions = new QHBoxLayout;
    actions->addWidget(create);
    actions->addWidget(copy_);
    left->addLayout(actions);
    split->addWidget(browser);
    details_ = new QWidget;
    auto detail = new QVBoxLayout(details_);
    detail->setContentsMargins(8, 0, 0, 0);
    auto identity = new QGridLayout;
    identity->setColumnStretch(1, 2);
    identity->setColumnStretch(3, 1);
    scenario_ = choice("customScenario");
    terrain_ = choice("customTerrain", false);
    for (int key = 0; key < 256; ++key)
        scenario_->addItem(
            QString("%1 · %2").arg(key).arg(project_->encounters()->scenarioLabel(key)), key);
    for (int i = 0; i < project_->encounters()->slotsPerPool(); ++i)
        terrain_->addItem(project_->encounters()->poolSlotLabel(i), i);
    identity->addWidget(new QLabel("Scenario"), 0, 0);
    identity->addWidget(scenario_, 0, 1);
    identity->addWidget(new QLabel("Encounter pool"), 0, 2);
    identity->addWidget(terrain_, 0, 3);
    label_ = new QLineEdit;
    label_->setObjectName("customLabel");
    label_->setMaxLength(24);
    enabled_ = new QCheckBox("Use this squad for encounters");
    enabled_->setObjectName("customEnabled");
    enabled_->setToolTip("When checked, this squad replaces the normal creature choices for its "
                         "scenario and terrain slot. When unchecked, the normal pool applies "
                         "and this squad remains saved for later use.");
    identity->addWidget(new QLabel("Encounter card"), 1, 0);
    identity->addWidget(label_, 1, 1, 1, 2);
    identity->addWidget(enabled_, 1, 3);
    detail->addLayout(identity);
    auto tabs = new QTabWidget;
    tabs->setObjectName("customDetailsTabs");
    detail->addWidget(tabs, 1);
    auto memberPage = new QWidget;
    auto memberLayout = new QVBoxLayout(memberPage);
    memberEditor_ = new SquadMembersEditor(project_, "custom", 2, 2);
    memberLayout->addWidget(memberEditor_);
    connect(memberEditor_, &SquadMembersEditor::edited, this, [this] { preview(); });
    auto levels = new QLabel("Level offsets are added to the encounter level. Results outside "
                             "0–255 wrap, matching the game.");
    levels->setWordWrap(true);
    memberLayout->addWidget(levels);
    memberLayout->addStretch();
    auto memberScroll = new QScrollArea;
    memberScroll->setWidgetResizable(true);
    memberScroll->setFrameShape(QFrame::NoFrame);
    memberLayout->setSizeConstraint(QLayout::SetMinimumSize);
    memberScroll->setWidget(memberPage);
    tabs->addTab(memberScroll, "Members and formation");
    auto behavior = new QWidget;
    auto behaviorLayout = new QVBoxLayout(behavior);
    auto form = new QFormLayout;
    persuasion_ = choice("customPersuasion", false);
    persuasion_->addItem("Use configured chance — last eligible target", 1);
    persuasion_->addItem("Original game calculation", 0);
    chance_ = spin("customChance", 0, 100);
    chance_->setSuffix("%");
    retreat_ = spin("customRetreat", 0, 100);
    retreat_->setSuffix("%");
    retreat_->setSpecialValueText("Never");
    form->addRow("Talk behavior", persuasion_);
    form->addRow("Base chance", chance_);
    form->addRow("Retreat at or below HP", retreat_);
    behaviorLayout->addLayout(form);
    auto affinityTitle = new QLabel("Talk / Persuade — player leader affinity bonuses");
    affinityTitle->setObjectName("customAffinityLabel");
    behaviorLayout->addWidget(affinityTitle);
    auto bonusTable = new QTableWidget(3, 2);
    bonusTable->setHorizontalHeaderLabels({"Player leader class", "Chance bonus"});
    bonusTable->verticalHeader()->hide();
    bonusTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bonusTable->setMaximumHeight(146);
    bonusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < 3; ++i) {
        bonusTable->setRowHeight(i, 36);
        auto cl = choice(QString("customBonusClass_%1").arg(i));
        bonusClasses_ << cl;
        auto bonus = spin(QString("customBonus_%1").arg(i), 1, 100);
        bonus->setSuffix("%");
        bonuses_ << bonus;
        bonusTable->setCellWidget(i, 0, cl);
        bonusTable->setCellWidget(i, 1, bonus);
        connect(cl, &QComboBox::currentIndexChanged, this, [this] { preview(); });
        connect(bonus, &QSpinBox::valueChanged, this, [this] { preview(); });
    }
    behaviorLayout->addWidget(bonusTable);
    auto talkHint =
        new QLabel("Configured chance uses the Base chance plus one matching leader-class bonus, "
                   "capped at 100%. Talk fails while another eligible "
                   "target remains. Failed Talk does not force retreat. Fight and Talk "
                   "remain available until the encounter resolves.");
    talkHint->setWordWrap(true);
    behaviorLayout->addWidget(talkHint);
    auto rewards = new QTableWidget(3, 3);
    rewards->setHorizontalHeaderLabels({"Victory reward", "Weight", "Share"});
    rewards->verticalHeader()->hide();
    rewards->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rewards->setMaximumHeight(150);
    rewards->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < 3; ++i) {
        rewards->setRowHeight(i, 36);
        auto item = choice(QString("customReward_%1").arg(i));
        item->addItem("None", 0);
        for (const auto kind : {Table::Items, Table::Consumables})
            for (int id = 1; id < project_->records(kind).size(); ++id)
                item->addItem(QString("%1 %2 · %3")
                                  .arg(kind == Table::Items ? "Equipment" : "Consumable")
                                  .arg(id)
                                  .arg(QString(project_->record(kind, id).name).replace('\n', ' ')),
                              (kind == Table::Items ? 0x8000 : 0) | id);
        rewards_ << item;
        auto weight = spin(QString("customWeight_%1").arg(i), 0, 65535);
        weights_ << weight;
        auto odds = new QLabel;
        odds->setMargin(6);
        odds_ << odds;
        rewards->setCellWidget(i, 0, item);
        rewards->setCellWidget(i, 1, weight);
        rewards->setCellWidget(i, 2, odds);
        connect(item, &QComboBox::currentIndexChanged, this, [this] { preview(); });
        connect(weight, &QSpinBox::valueChanged, this, [this] { preview(); });
    }
    behaviorLayout->addWidget(rewards);
    auto rewardHint = new QLabel("Zero weight excludes an outcome. None with positive weight can "
                                 "give no reward. Persuasion gives no victory reward.");
    rewardHint->setWordWrap(true);
    behaviorLayout->addWidget(rewardHint);
    behaviorLayout->addStretch();
    auto behaviorScroll = new QScrollArea;
    behaviorScroll->setWidgetResizable(true);
    behaviorScroll->setFrameShape(QFrame::NoFrame);
    behaviorLayout->setSizeConstraint(QLayout::SetMinimumSize);
    behaviorScroll->setWidget(behavior);
    tabs->addTab(behaviorScroll, "Battle and rewards");
    auto buttons = new QHBoxLayout;
    auto applyButton = new QPushButton("Apply squad");
    applyButton->setObjectName("customApply");
    restore_ = new QPushButton("Restore loaded squad");
    restore_->setObjectName("customRestore");
    remove_ = new QPushButton("Remove squad");
    remove_->setObjectName("customRemove");
    buttons->addWidget(applyButton);
    buttons->addWidget(restore_);
    buttons->addStretch();
    buttons->addWidget(remove_);
    detail->addLayout(buttons);
    notice_ = new QLabel;
    notice_->setObjectName("customNotice");
    notice_->setWordWrap(true);
    detail->addWidget(notice_);
    split->addWidget(details_);
    split->setSizes({245, 900});
    connect(create, &QPushButton::clicked, this, [this] { newDraft(false); });
    connect(copy_, &QPushButton::clicked, this, [this] { newDraft(true); });
    connect(applyButton, &QPushButton::clicked, this, [this] { apply(); });
    connect(remove_, &QPushButton::clicked, this, [this] { remove(false); });
    connect(restore_, &QPushButton::clicked, this, [this] { remove(true); });
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(list_, &QListWidget::currentRowChanged, this, [this] { showSelected(); });
    connect(label_, &QLineEdit::textChanged, this, [this] { preview(); });
    connect(scenario_, &QComboBox::currentIndexChanged, this, [this] { preview(); });
    connect(terrain_, &QComboBox::currentIndexChanged, this, [this] { preview(); });
    connect(enabled_, &QCheckBox::toggled, this, [this] { preview(); });
    connect(persuasion_, &QComboBox::currentIndexChanged, this, [this] { preview(); });
    connect(chance_, &QSpinBox::valueChanged, this, [this] { preview(); });
    connect(retreat_, &QSpinBox::valueChanged, this, [this] { preview(); });
    connect(undo, &QUndoStack::indexChanged, this, [this] { refresh(); });
    loading_ = false;
    refresh();
}
void CustomSquadEditor::refresh() {
    const ViewPosition keepView(this);
    const QSignalBlocker terrainBlock(terrain_);
    const int terrain = terrain_->currentData().toInt();
    terrain_->clear();
    for (int i = 0; i < project_->encounters()->slotsPerPool(); ++i)
        terrain_->addItem(project_->encounters()->poolSlotLabel(i), i);
    terrain_->setCurrentIndex(qMax(0, terrain_->findData(terrain)));
    loading_ = true;
    QSignalBlocker block(list_);
    list_->clear();
    const auto &profiles = project_->encounters()->customSquads().profiles();
    int selectedRow = -1;
    for (auto it = profiles.begin(); it != profiles.end(); ++it) {
        const auto &s = it.value();
        auto item =
            new QListWidgetItem(QString("%1%2 · %3\n%4 · %5")
                                    .arg(s.enabled ? "" : "[Disabled] ")
                                    .arg(s.scenarioKey)
                                    .arg(encounterSquadName(*project_, it.key()))
                                    .arg(project_->encounters()->scenarioLabel(s.scenarioKey))
                                    .arg(project_->encounters()->poolSlotLabel(s.terrainSlot)),
                                list_);
        item->setData(Qt::UserRole, it.key());
        item->setToolTip(item->text());
        if (it.key() == editing_)
            selectedRow = list_->count() - 1;
    }
    for (auto box : rewards_) {
        const QSignalBlocker blocker(box);
        box->clear();
        box->addItem("None", 0);
        for (const auto kind : {Table::Items, Table::Consumables})
            for (int id = 1; id < project_->records(kind).size(); ++id)
                box->addItem(QString("%1 %2 · %3")
                                 .arg(kind == Table::Items ? "Equipment" : "Consumable")
                                 .arg(id)
                                 .arg(project_->record(kind, id).name),
                             (kind == Table::Items ? 0x8000 : 0) | id);
    }
    for (auto box : bonusClasses_) {
        box->clear();
        if (bonusClasses_.contains(box))
            box->addItem("None", 0);
        for (int id = 2; id < project_->records(Table::Classes).size(); ++id)
            box->addItem(QString("%1 · %2").arg(id).arg(
                             QString(project_->record(Table::Classes, id).name).replace('\n', ' ')),
                         id);
    }
    list_->setCurrentRow(selectedRow >= 0 ? selectedRow : list_->count() ? 0 : -1);
    loading_ = false;
    filter();
    showSelected();
}
void CustomSquadEditor::filter() {
    const auto text = search_->text().trimmed();
    int first = -1;
    for (int i = 0; i < list_->count(); ++i) {
        auto item = list_->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        if (first < 0 && !item->isHidden())
            first = i;
    }
    if (!list_->currentItem() || list_->currentItem()->isHidden())
        list_->setCurrentRow(first);
    if (!list_->currentItem())
        showSelected();
}
void CustomSquadEditor::showSelected() {
    if (loading_)
        return;
    const auto item = list_->currentItem();
    const bool valid = item && !item->isHidden();
    details_->setEnabled(valid);
    copy_->setEnabled(valid);
    remove_->setEnabled(valid);
    restore_->setEnabled(valid);
    editing_ = valid ? item->data(Qt::UserRole).toInt() : -1;
    if (valid)
        display(project_->encounters()->customSquads().profiles().value(editing_));
    else
        notice_->setText("Create a squad, or select one from the list.");
}
void CustomSquadEditor::display(const CustomSquad &s) {
    loading_ = true;
    scenario_->setCurrentIndex(scenario_->findData(s.scenarioKey));
    terrain_->setCurrentIndex(terrain_->findData(s.terrainSlot));
    label_->setText(s.label);
    enabled_->setChecked(s.enabled);
    memberEditor_->setMembers(s.members);
    persuasion_->setCurrentIndex(persuasion_->findData(s.fixedPersuasion ? 1 : 0));
    chance_->setValue(s.chance);
    retreat_->setValue(s.retreatHp);
    for (int i = 0; i < 3; ++i) {
        bonusClasses_[i]->setCurrentIndex(
            bonusClasses_[i]->findData(i < s.bonuses.size() ? s.bonuses[i].first : 0));
        bonuses_[i]->setValue(i < s.bonuses.size() ? s.bonuses[i].second : 10);
        rewards_[i]->setCurrentIndex(rewards_[i]->findData(s.rewards.outcomes[i]));
        weights_[i]->setValue(s.rewards.weights[i]);
    }
    notice_->clear();
    loading_ = false;
    preview();
}
CustomSquad CustomSquadEditor::collect() const {
    CustomSquad s;
    s.scenarioKey = selected(scenario_);
    s.terrainSlot = selected(terrain_);
    s.label = label_->text();
    s.enabled = enabled_->isChecked();
    s.members = memberEditor_->members();
    s.fixedPersuasion = selected(persuasion_) == 1;
    s.chance = chance_->value();
    s.retreatHp = retreat_->value();
    for (int i = 0; i < 3; ++i) {
        const int id = selected(bonusClasses_[i]);
        if (id)
            s.bonuses << qMakePair(id, bonuses_[i]->value());
        s.rewards.outcomes[i] = selected(rewards_[i]);
        s.rewards.weights[i] = weights_[i]->value();
    }
    return s;
}
void CustomSquadEditor::newDraft(bool copy) {
    try {
        CustomSquad s = copy ? collect() : CustomSquad{};
        const auto &profiles = project_->encounters()->customSquads().profiles();
        const int start = CustomSquads::key(s.scenarioKey, s.terrainSlot);
        int free = -1;
        for (int i = 0; i < 8192; ++i) {
            const int key = (start + i) % 8192;
            if (key % 32 < project_->encounters()->slotsPerPool() && !profiles.contains(key)) {
                free = key;
                break;
            }
        }
        if (free < 0)
            throw Error("Every scenario and terrain slot already has a squad");
        s.scenarioKey = free / 32;
        s.terrainSlot = free % 32;
        editing_ = -1;
        QSignalBlocker block(list_);
        list_->setCurrentRow(-1);
        details_->setEnabled(true);
        copy_->setEnabled(false);
        remove_->setEnabled(false);
        restore_->setEnabled(false);
        display(s);
        notice_->setText("New squad draft — Apply squad saves it to the project.");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void CustomSquadEditor::apply() {
    try {
        const auto row =
            CustomSquad::parse(collect().json(), int(project_->records(Table::Classes).size()));
        const int key = CustomSquads::key(row.scenarioKey, row.terrainSlot);
        auto next = *project_->encounters();
        if (key != editing_ && next.customSquads().profiles().contains(key))
            throw Error("That scenario and terrain slot already has a squad");
        if (editing_ >= 0 && key != editing_)
            next.removeCustomSquad(editing_ / 32, editing_ % 32);
        next.setCustomSquad(row, int(project_->records(Table::Classes).size()));
        next.customSquads().validateFormations(project_->classData());
        editing_ = key;
        commit_(next, "Edit custom encounter squad");
        refresh();
        notice_->setText("Squad saved to the project.");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void CustomSquadEditor::remove(bool restore) {
    if (editing_ < 0)
        return;
    try {
        auto next = *project_->encounters();
        const int key = editing_;
        next.removeCustomSquad(key / 32, key % 32);
        if (restore) {
            const auto original =
                CustomSquads::load(project_->rom(), int(project_->records(Table::Classes).size()));
            if (original.profiles().contains(key))
                next.setCustomSquad(original.profiles()[key],
                                    int(project_->records(Table::Classes).size()));
        }
        commit_(next, restore ? "Restore loaded custom squad" : "Remove custom encounter squad");
        refresh();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void CustomSquadEditor::preview() {
    if (loading_)
        return;
    notice_->clear();
    const bool fixed = persuasion_->currentData().toInt() == 1;
    chance_->setEnabled(fixed);
    for (int i = 0; i < 3; ++i) {
        bonusClasses_[i]->setEnabled(fixed);
        bonuses_[i]->setEnabled(fixed && bonusClasses_[i]->currentData().toInt() != 0);
    }
    const auto total = weights_[0]->value() + weights_[1]->value() + weights_[2]->value();
    for (int i = 0; i < 3; ++i)
        odds_[i]->setText(
            total ? QString::number(100.0 * weights_[i]->value() / total, 'f', 1) + "%" : "—");
    try {
        const auto s = collect();
        if (editing_ < 0 ||
            project_->encounters()->customSquads().profiles().value(editing_).json() != s.json())
            notice_->setText("Unapplied changes — Apply squad updates the project.");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
