#include "app/attack_editor.h"
#include "app/actor_pose_editor.h"
#include "app/attack_effect_editor.h"
#include "app/content_refresh.h"
#include "engine/combat_animation_edit.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace lordly {
namespace {
class AttackChange final : public QUndoCommand {
    Project *project_;
    QJsonObject before_, after_;

  public:
    AttackChange(Project *p, QString title, QJsonObject before, QJsonObject after)
        : QUndoCommand(title), project_(p), before_(std::move(before)), after_(std::move(after)) {}
    void undo() override {
        project_->setAttacks(before_);
    }
    void redo() override {
        project_->setAttacks(after_);
    }
};
class AttackAssignments final : public QUndoCommand {
    Project *project_;
    int id_;
    QByteArray before_, after_;

  public:
    AttackAssignments(Project *p, int id, QByteArray before, QByteArray after)
        : QUndoCommand("Change class attacks"), project_(p), id_(id), before_(std::move(before)),
          after_(std::move(after)) {}
    void undo() override {
        project_->replaceBytes(Table::Classes, id_, before_);
    }
    void redo() override {
        project_->replaceBytes(Table::Classes, id_, after_);
    }
};
QLabel *text(const QString &value, QLayout *layout = nullptr) {
    auto label = new QLabel(value);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    if (layout)
        layout->addWidget(label);
    return label;
}
QComboBox *choices(const QMap<int, QString> &values, const char *name) {
    auto box = new QComboBox;
    box->setObjectName(name);
    for (auto it = values.begin(); it != values.end(); ++it)
        box->addItem(it.value(), it.key());
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(18);
    return box;
}
void choose(QComboBox *box, int value) {
    if (box->findData(value) < 0)
        box->addItem(QString("Unknown ID %1 (preserved)").arg(value), value);
    box->setCurrentIndex(box->findData(value));
}
QSpinBox *number(const char *name, int min, int max, int initial) {
    auto box = new QSpinBox;
    box->setObjectName(name);
    box->setRange(min, max);
    box->setValue(initial);
    box->setKeyboardTracking(false);
    return box;
}
int classAction(const Project &p, int id, int row) {
    const auto bytes = p.record(Table::Classes, id).bytes;
    int action = u8(bytes, 55 + 2 * row), owner = u8(bytes, 69);
    if (action == 255 && owner && owner < p.records(Table::Classes).size())
        action = u8(p.record(Table::Classes, owner).bytes, 55 + 2 * row);
    return action;
}
QString attackName(const AttackData &attacks, int id) {
    return id > 0 && id < attacks.entries().size() ? AttackData::displayName(attacks.entry(id))
                                                   : QString("ID %1").arg(id);
}
int nativeSpellIndex(const Attack &a) {
    const int id = a.animation;
    return quint8(id - 17) < 28   ? quint8(id + 80)
           : quint8(id + 110) < 8 ? quint8(id + 235)
           : quint8(id - 55) < 36 ? quint8(id + 201)
           : quint8(id - 91) < 54 ? quint8(id + 204)
           : quint8(id + 102) < 3 ? quint8(id + 138)
                                  : 1;
}
int nativePattern(const Project &project, const Attack &a) {
    if (!u8(a.data, 2))
        return 0;
    if (u8(a.data, 0) < 2)
        return 1;
    if (u8(a.data, 0) == 3) {
        const int index = nativeSpellIndex(a);
        return u8(project.rom().bytes(), 0x212181 + 2 * index) & 15;
    }
    const int selector = u8(a.data, 2);
    return selector == 2   ? 6
           : selector == 3 ? 2
           : selector == 4 ? 12
           : selector == 5 ? 10
           : selector == 6 ? 5
                           : 1;
}
QString nativeSideName(const Attack &a) {
    if (!u8(a.data, 2))
        return "No targets";
    if (u8(a.data, 0) > 5)
        return QString("Unknown target side (execution ID %1)").arg(u8(a.data, 0));
    // Native target builder 00238D94 uses 0020C364's spell-index predicate,
    // not D3. Changing a spell's main effect alone does not change its side.
    const int index = nativeSpellIndex(a);
    return u8(a.data, 0) >= 2 && ((index >= 32 && index <= 35) || index == 103)
               ? "Allies (including user)"
               : "Opponents";
}
QString nativePatternName(int pattern) {
    switch (pattern) {
    case 0: return "No targets";
    case 1:
    case 8: return "One target";
    case 2:
    case 3:
    case 4:
    case 7: return "Cross through target";
    case 5:
    case 6:
    case 11: return "3 × 3 area around target";
    case 9:
    case 10: return "Six strikes (game-selected targets)";
    case 12: return "User's opposing column (eligible targets)";
    case 14: return "One opponent";
    default: return QString("Unknown pattern ID %1").arg(pattern);
    }
}
QString nativePatternDetail(int pattern) {
    switch (pattern) {
    case 0: return "This entry has no direct targets.";
    case 1:
    case 8:
    case 14: return "The game chooses one eligible target.";
    case 2:
    case 3:
    case 4:
    case 7:
        return "Affects the chosen position and its four adjacent positions. "
               "The original calculation gives the outer positions reduced amounts.";
    case 5:
    case 6:
        return "Affects the chosen position and its eight surrounding positions. "
               "The original calculation gives the outer positions reduced amounts.";
    case 11:
        return "Affects the chosen position and its eight surrounding positions, "
               "using the same calculation rule at each position.";
    case 9:
    case 10:
        return "Selects six strikes from eligible targets around the chosen position. "
               "The same unit can be hit more than once. The diagram shows the possible area, "
               "not six fixed targets.";
    case 12:
        return "Selects eligible opponents in the user's column. The diagram assumes the user "
               "is in the center column; the game's eligibility checks still apply.";
    default: return QString("Pattern ID %1 is preserved; its area is not identified.").arg(pattern);
    }
}
QPair<QString, QString> nativeStatusRule(const Attack &a) {
    const int kind = u8(a.data, 6), effect = u8(a.data, 7), activation = u8(a.data, 9);
    if (kind == 12)
        return {"From equipped weapon",
                "Uses the equipped weapon's on-hit effect and activation rule. "
                "An additional effect selected here is separate from the weapon's effect."};
    if (kind == 0 || kind == 255 || kind == 11)
        return {"None", "No additional status effect is built into this attack. "
                        "Weapon on-hit effects remain separate."};
    if (kind != 8)
        return {QString("Unknown secondary rule ID %1").arg(kind),
                "This unidentified rule is preserved. Raw field IDs are available in Advanced."};
    const auto name = effect == 7 ? QString("Banish eligible undead")
                                  : AttackData::statuses().value(effect, QString("Unknown effect ID %1").arg(effect));
    QString condition;
    switch (activation) {
    case 2: condition = "After each target attempt; no extra chance roll."; break;
    case 4: condition = "After a successful hit; chance is half the calculated hit chance, "
                        "rounded down (80% hit chance → 40% effect chance)."; break;
    case 5: condition = "After a successful hit; no extra chance roll (100% of eligible hits)."; break;
    case 6: condition = "After a successful hit whose calculated amount is at least the target's "
                        "remaining HP; no extra chance roll."; break;
    case 11: condition = "After a successful hit; chance is one quarter of the calculated hit chance, "
                         "rounded down (80% hit chance → 20% effect chance)."; break;
    default: condition = QString("Unknown activation rule ID %1 (preserved).").arg(activation); break;
    }
    return {name, condition + " Target eligibility and immunity still apply."};
}
QPair<QString, QString> nativeAfterRule(const Attack &a) {
    const int kind = u8(a.data, 6), effect = u8(a.data, 7), calculation = u8(a.data, 8);
    if (kind == 0 || kind == 255 || kind == 8 || kind == 12)
        return {"None", "This attack has no built-in drain or recoil."};
    if (kind != 11)
        return {QString("Unknown secondary rule ID %1").arg(kind), "The original rule is preserved."};
    const auto name = effect == 1 ? QString("Damage the attacker (recoil)")
                      : effect == 2 ? QString("Heal the attacker (drain)")
                                    : QString("Unknown after-effect ID %1").arg(effect);
    const auto amount = calculation == 6 ? QString("Last target's calculated amount, including any failed-hit reduction; "
                                                   "not total HP lost or damage capped to remaining HP")
                         : calculation == 5 ? QString("Attacker's alternate physical power / 6")
                                             : AttackData::calculations().value(calculation,
                                                   QString("Unknown calculation ID %1").arg(calculation));
    return {name, amount + ". Applied once after the target loop. " +
                      (effect == 2 ? "Healing is capped by the attacker's missing HP."
                       : effect == 1 ? "Recoil can defeat the attacker." : "")};
}
} // namespace
AttackEditor::AttackEditor(Project *p, QUndoStack *undo, std::function<void()> changed,
                           QWidget *parent)
    : QWidget(parent), project_(p), undo_(undo), changed_(std::move(changed)) {
    setObjectName("attackEditor");
    project_->loadAttacks();
    opened_ = *project_->attacks();
    auto layout = new QVBoxLayout(this);
    auto split = new QSplitter;
    layout->addWidget(split);
    auto left = new QWidget;
    auto leftLayout = new QVBoxLayout(left);
    search_ = new QLineEdit;
    search_->setObjectName("attackSearch");
    search_->setPlaceholderText("Search attacks, effects, elements or IDs…");
    search_->setClearButtonEnabled(true);
    leftLayout->addWidget(search_);
    list_ = new QListWidget;
    list_->setObjectName("attackList");
    list_->setTextElideMode(Qt::ElideNone);
    leftLayout->addWidget(list_, 1);
    auto actions = new QHBoxLayout;
    auto add = new QPushButton("Create attack…"), copy = new QPushButton("Duplicate…");
    add->setObjectName("createAttack");
    copy->setObjectName("duplicateAttack");
    actions->addWidget(add);
    actions->addWidget(copy);
    leftLayout->addLayout(actions);
    capacity_ = text({}, leftLayout);
    split->addWidget(left);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    details_ = new QWidget;
    auto right = new QVBoxLayout(details_);
    auto titleRow = new QHBoxLayout;
    name_ = text({});
    auto renameButton = new QPushButton("Rename…");
    renameButton->setObjectName("renameAttack");
    titleRow->addWidget(name_, 1);
    titleRow->addWidget(renameButton);
    right->addLayout(titleRow);
    shared_ = text({}, right);
    relationship_ = text({}, right);
    relationship_->setTextFormat(Qt::RichText);
    auto behavior = new QGroupBox("What this attack does");
    effectPanel_ = behavior;
    auto behaviorLayout = new QVBoxLayout(behavior);
    auto basic = new QFormLayout;
    auto elements = AttackData::elements();
    elements.remove(8);
    elements.remove(255);
    element_ = choices(elements, "attackElement");
    effect_ = choices(AttackData::effects(), "attackEffect");
    basic->addRow("Element", element_);
    basic->addRow("Main effect", effect_);
    behaviorLayout->addLayout(basic);
    elementHelp_ = text({}, behaviorLayout);
    amountPanel_ = new QWidget;
    auto amountLayout = new QFormLayout(amountPanel_);
    amountLayout->setContentsMargins(0, 0, 0, 0);
    auto calculations = AttackData::calculations();
    calculations.remove(6); // Supplied amount belongs to native secondary rules.
    calculation_ = choices(calculations, "attackCalculation");
    power_ = number("attackPower", 0, 1000, 100);
    power_->setSuffix(" %");
    amountLayout->addRow("Calculation", calculation_);
    amountLayout->addRow("Incoming power", power_);
    amountLayout->addRow(text("100% keeps incoming power unchanged. 200% doubles power before "
                              "defense and resistance. Half / 80% calculation rules reduce the "
                              "calculated amount afterward. HP-based rules ignore power."));
    behaviorLayout->addWidget(amountPanel_);
    right->addWidget(behavior);
    auto targeting = new QGroupBox("Who it affects");
    auto targetLayout = new QVBoxLayout(targeting);
    auto targetForm = new QFormLayout;
    side_ = choices({{0, "Original target side"},
                     {1, "Opponents"},
                     {2, "Allies (including user)"},
                     {3, "User only"}},
                    "attackSide");
    pattern_ = choices({{0, "Original target pattern"},
                        {1, "One target"},
                        {2, "All occupied positions"},
                        {3, "Front row"},
                        {4, "Middle row"},
                        {5, "Rear row"},
                        {6, "Target's column"},
                        {7, "Target's row"},
                        {8, "Cross through target"},
                        {9, "User only"},
                        {12, "Custom positions…"}},
                       "attackPattern");
    targetForm->addRow("Target side", side_);
    targetForm->addRow("Pattern", pattern_);
    auto targetingRow = new QHBoxLayout;
    targetingRow->addLayout(targetForm, 1);
    targetLayout->addLayout(targetingRow);
    auto grid = new QGridLayout;
    for (int row = 0; row < 3; ++row) {
        grid->addWidget(new QLabel(QStringList{"Front", "Middle", "Rear"}[row]), row, 0);
        for (int col = 0; col < 3; ++col) {
            auto cell = new QCheckBox;
            cell->setObjectName(QString("attackCell%1").arg(row * 3 + col));
            cell->setText(QString::number(col + 1));
            cell->setMinimumSize(64, 34);
            grid->addWidget(cell, row, col + 1);
            cells_ << cell;
            connect(cell, &QCheckBox::clicked, this, [this] {
                int mask = 0;
                for (int i = 0; i < cells_.size(); ++i)
                    if (cells_[i]->isChecked())
                        mask |= 1 << i;
                if (!mask) {
                    showAttack();
                    return;
                }
                edit("Change target positions", [mask](Attack &a) {
                    a.targetPattern = 12;
                    a.targetMask = mask;
                });
            });
        }
    }
    targetingRow->addSpacing(16);
    targetingRow->addLayout(grid);
    targetHelp_ = text({}, targetLayout);
    right->addWidget(targeting);
    auto secondary = new QGroupBox("Additional status effect");
    auto secondaryLayout = new QVBoxLayout(secondary);
    auto statusForm = new QFormLayout;
    auto statuses = AttackData::statuses();
    statuses[-1] = "Original additional effect";
    status_ = choices(statuses, "attackStatus");
    statusForm->addRow("Effect", status_);
    secondaryLayout->addLayout(statusForm);
    conditionPanel_ = new QWidget;
    auto conditionLayout = new QFormLayout(conditionPanel_);
    conditionLayout->setContentsMargins(0, 0, 0, 0);
    condition_ = choices({{0, "After each target attempt"}, {1, "After a successful hit"}},
                         "attackCondition");
    chance_ = number("attackChance", 0, 100, 100);
    chance_->setSuffix(" %");
    conditionLayout->addRow("Apply when", condition_);
    conditionLayout->addRow("Chance per eligible target", chance_);
    secondaryLayout->addWidget(conditionPanel_);
    nativeStatus_ = text({}, secondaryLayout);
    nativeStatus_->setObjectName("attackNativeStatus");
    right->addWidget(secondary);
    auto aftermath = new QGroupBox("Drain and recoil");
    auto afterLayout = new QVBoxLayout(aftermath);
    auto afterForm = new QFormLayout;
    after_ = choices({{-1, "Original drain / recoil"},
                      {0, "None"},
                      {1, "Heal the attacker (drain)"},
                      {2, "Damage the attacker (recoil)"}},
                     "attackAfter");
    afterForm->addRow("After the attack", after_);
    afterLayout->addLayout(afterForm);
    afterPanel_ = new QWidget;
    auto basisLayout = new QFormLayout(afterPanel_);
    basisLayout->setContentsMargins(0, 0, 0, 0);
    basis_ = choices({{0, "Total HP lost by all main-effect targets"},
                      {1, "Last target's calculated amount"},
                      {2, "Attacker HP before this attack"},
                      {3, "Attacker maximum HP"}},
                     "attackAfterBasis");
    afterPercent_ = number("attackAfterPercent", 0, 1000, 100);
    afterPercent_->setSuffix(" %");
    basisLayout->addRow("Amount based on", basis_);
    basisLayout->addRow("Share of that amount", afterPercent_);
    afterLayout->addWidget(afterPanel_);
    nativeAfter_ = text({}, afterLayout);
    nativeAfter_->setObjectName("attackNativeAfter");
    right->addWidget(aftermath);
    auto used = new QGroupBox("Used by these classes");
    auto usedLayout = new QVBoxLayout(used);
    text("Front, middle and rear assignments are the same settings shown in Classes. Count 255 "
         "inherits the current class's count. A row marked “via template” reaches this attack "
         "through an elemental spell template.",
         usedLayout);
    users_ = new QTableWidget(0, 7);
    users_->setObjectName("attackUsers");
    users_->setHorizontalHeaderLabels(
        {"Class", "Front", "Count", "Middle", "Count", "Rear", "Count"});
    users_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    users_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    users_->verticalHeader()->hide();
    users_->setMinimumHeight(180);
    usedLayout->addWidget(users_);
    auto noUsers = text("No classes use this attack yet.", usedLayout);
    noUsers->setObjectName("attackNoUsers");
    auto assign = new QPushButton("Assign to a class…");
    assign->setObjectName("assignAttack");
    usedLayout->addWidget(assign);
    right->addWidget(used);
    auto animation = new QGroupBox("Animation");
    auto animationLayout = new QVBoxLayout(animation);
    auto animationRow = new QHBoxLayout;
    previewClass_ = new QComboBox;
    previewClass_->setObjectName("attackPreviewClass");
    previewClass_->setMinimumContentsLength(16);
    previewClass_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto previewButton = new QPushButton("Preview class motion…"),
         animationButton = new QPushButton("Edit in Combat Animations…");
    previewButton->setObjectName("attackPreview");
    animationButton->setObjectName("attackEditAnimation");
    animationRow->addWidget(previewClass_, 1);
    animationRow->addWidget(previewButton);
    animationRow->addWidget(animationButton);
    animationLayout->addLayout(animationRow);
    text("Class motion belongs to a class, side and artwork variant. Two classes using the "
         "same attack can use different body animations. Edit the attack's visual effect below.",
         animationLayout);
    auto effectButton = new QPushButton("Preview / edit attack effect…");
    effectButton->setObjectName("attackEditEffect");
    animationLayout->addWidget(effectButton);
    connect(effectButton, &QPushButton::clicked, this, &AttackEditor::editEffect);
    right->addWidget(animation);
    auto example = new QGroupBox("Before / after example");
    auto exampleLayout = new QVBoxLayout(example);
    auto exampleGrid = new QGridLayout;
    const QStringList exampleLabels{"Incoming power", "Defense",     "Luck difference",
                                    "Resistance (%)", "Attacker HP", "Target HP",
                                    "Target max HP"};
    const int defaults[] = {100, 40, 0, 0, 300, 200, 400};
    for (int i = 0; i < 7; ++i) {
        auto input = number(qPrintable(QString("attackExample%1").arg(i)), i == 2 ? -999 : 0,
                            i == 3 ? 100 : 9999, defaults[i]);
        example_ << input;
        exampleGrid->addWidget(new QLabel(exampleLabels[i]), i / 2, (i % 2) * 2);
        exampleGrid->addWidget(input, i / 2, (i % 2) * 2 + 1);
        connect(input, &QSpinBox::valueChanged, this, [this] { comparison(); });
    }
    exampleLayout->addLayout(exampleGrid);
    comparison_ = text({}, exampleLayout);
    text("Successful, noncritical main effect at neutral random variation. Power and defense are "
         "already calculated battle scores. The resistance input is for the selected element. "
         "Formation bonuses, immunity, global event rules and additional effects are outside this "
         "example.",
         exampleLayout);
    right->addWidget(example);
    auto advancedToggle = new QCheckBox("Advanced: preserved field IDs");
    right->addWidget(advancedToggle);
    advanced_ = text({}, right);
    advanced_->hide();
    connect(advancedToggle, &QCheckBox::toggled, advanced_, &QWidget::setVisible);
    notice_ = text({}, right);
    notice_->setObjectName("attackNotice");
    right->addStretch();
    scroll->setWidget(details_);
    split->addWidget(scroll);
    split->setStretchFactor(1, 1);
    split->setSizes({300, 950});
    connect(search_, &QLineEdit::textChanged, this, [this] { filter(); });
    connect(list_, &QListWidget::currentRowChanged, this, [this] {
        if (!refreshing_)
            showAttack();
    });
    connect(renameButton, &QPushButton::clicked, this, &AttackEditor::rename);
    connect(add, &QPushButton::clicked, this, [this] { create(false); });
    connect(copy, &QPushButton::clicked, this, [this] { create(true); });
    connect(assign, &QPushButton::clicked, this, &AttackEditor::assignClass);
    connect(previewButton, &QPushButton::clicked, this, &AttackEditor::preview);
    connect(animationButton, &QPushButton::clicked, this,
            [this] { emit editAnimation(previewClass_->currentData().toInt(), selected()); });
    connect(relationship_, &QLabel::linkActivated, this,
            [this](const QString &id) { selectAttack(id.toInt()); });
    const auto field = [this](QComboBox *box, int offset, const QString &title) {
        connect(box, &QComboBox::activated, this, [this, box, offset, title] {
            int n = box->currentData().toInt();
            edit(title, [=](Attack &a) {
                put8(a.data, offset, n);
                if (offset == 3) {
                    put8(a.data, 5, n == 2 ? 9 : 1);
                    if (n && !u8(a.data, 2))
                        put8(a.data, 2, 1);
                }
            });
        });
    };
    field(element_, 1, "Change attack element");
    field(effect_, 3, "Change attack effect");
    field(calculation_, 4, "Change attack calculation");
    connect(side_, &QComboBox::activated, this, [this] {
        const int n = side_->currentData().toInt();
        edit("Change target side", [=](Attack &a) {
            a.targetSide = n;
            if (n && !a.targetPattern)
                a.targetPattern = 1;
        });
    });
    connect(pattern_, &QComboBox::activated, this, [this] {
        const int n = pattern_->currentData().toInt();
        edit("Change target pattern", [=](Attack &a) {
            // Self is implied by pattern 9. Do not persist that implication as
            // a side override; clear the old UI's redundant override on exit.
            if (a.targetPattern == 9 && a.targetSide == 3 && n != 9)
                a.targetSide = 0;
            a.targetPattern = n;
            if (n == 12 && !a.targetMask)
                a.targetMask = 16;
        });
    });
    const auto choice = [this](QComboBox *box, int Attack::*member, const QString &title) {
        connect(box, &QComboBox::activated, this, [this, box, member, title] {
            const int n = box->currentData().toInt();
            edit(title, [=](Attack &a) { a.*member = n; });
        });
    };
    choice(status_, &Attack::status, "Change additional effect");
    choice(condition_, &Attack::condition, "Change effect condition");
    choice(after_, &Attack::afterEffect, "Change drain / recoil");
    choice(basis_, &Attack::afterBasis, "Change after-effect basis");
    const auto scalar = [this](QSpinBox *box, int Attack::*member, const QString &title) {
        connect(box, &QSpinBox::editingFinished, this, [this, box, member, title] {
            const int n = box->value();
            edit(title, [=](Attack &a) { a.*member = n; });
        });
    };
    scalar(power_, &Attack::powerPercent, "Change incoming power");
    scalar(chance_, &Attack::chance, "Change status chance");
    scalar(afterPercent_, &Attack::afterPercent, "Change after-effect amount");
    watchContentChanges(undo_, this, [this] {
        QTimer::singleShot(0, this, [this] {
            refresh();
            changed_();
        });
    });
    refresh();
}
int AttackEditor::selected() const {
    return list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toInt() : -1;
}
void AttackEditor::selectAttack(int id) {
    search_->clear();
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(Qt::UserRole).toInt() == id) {
            list_->setCurrentRow(i);
            return;
        }
}
void AttackEditor::filter() {
    for (int i = 0; i < list_->count(); ++i)
        list_->item(i)->setHidden(!list_->item(i)
                                       ->data(Qt::UserRole + 1)
                                       .toString()
                                       .contains(search_->text(), Qt::CaseInsensitive));
}
void AttackEditor::refresh() {
    if (refreshing_)
        return;
    QScopedValueRollback guard(refreshing_, true);
    int id = selected();
    if (id < 1)
        id = 1;
    const auto &attacks = *project_->attacks();
    list_->clear();
    for (int i = 1; i < attacks.entries().size(); ++i) {
        const auto &a = attacks.entry(i);
        auto item =
            new QListWidgetItem(QString("%1 · %2%3")
                                    .arg(i)
                                    .arg(AttackData::displayName(a),
                                         AttackData::isTemplate(a.behavior) ? "  [template]" : ""),
                                list_);
        item->setData(Qt::UserRole, i);
        item->setData(Qt::UserRole + 1, item->text() + " " +
                                            AttackData::effects().value(u8(a.data, 3)) + " " +
                                            AttackData::elements().value(u8(a.data, 1)));
    }
    list_->setCurrentRow(std::min(id, int(attacks.entries().size()) - 1) - 1);
    filter();
    capacity_->setText(QString("%1 / 254 attack IDs used · %2 available")
                           .arg(attacks.entries().size() - 1)
                           .arg(255 - attacks.entries().size()));
    findChild<QPushButton *>("createAttack")->setEnabled(attacks.entries().size() < 255);
    findChild<QPushButton *>("duplicateAttack")->setEnabled(attacks.entries().size() < 255);
    const int preview = previewClass_->currentData().toInt();
    previewClass_->clear();
    for (const auto &row : project_->records(Table::Classes))
        if (row.id)
            previewClass_->addItem(QString("%1 · %2").arg(row.id).arg(row.name), row.id);
    previewClass_->setCurrentIndex(std::max(0, previewClass_->findData(preview)));
    showAttack();
}
void AttackEditor::showAttack() {
    const int id = selected();
    if (id < 1)
        return;
    QScopedValueRollback guard(refreshing_, true);
    const auto &attacks = *project_->attacks();
    const auto &a = attacks.entry(id);
    name_->setText(
        QString("<h2>%1</h2>Attack %2").arg(AttackData::displayName(a).toHtmlEscaped()).arg(id));
    const auto shared = attacks.sharingName(id);
    QStringList names;
    for (int n : shared)
        if (n != id)
            names << QString::number(n);
    shared_->setText(
        shared.size() > 1
            ? "Name shared with attack IDs " + names.join(", ") +
                  ". Rename can change the shared name or give this attack an independent name."
            : "Independent name. Behavior edits affect every class using this attack.");
    const bool templ = AttackData::isTemplate(a.behavior);
    QStringList links;
    if (templ) {
        const auto targets = AttackData::templateTargets(a.behavior);
        for (auto it = targets.begin(); it != targets.end(); ++it)
            links << QString("%1 → <a href=\"%2\">%3</a>")
                         .arg(it.key() <= -249
                                  ? project_->record(Table::Items, -it.key()).name.toHtmlEscaped() +
                                        " equipped"
                              : it.key() < 0 ? "Fixed spell"
                                             : AttackData::elements().value(it.key()))
                         .arg(it.value())
                         .arg(attackName(attacks, it.value()).toHtmlEscaped());
        relationship_->setText(
            "<b>Elemental spell template.</b> This chooses an actual spell from equipment / "
            "element; edit that spell below to change its effect. Duplicating this template keeps "
            "these shared spell links; duplicate a resolved spell for independent effects. " +
            (a.behavior == 49 || a.behavior == 50
                 ? QString("Uses the strongest estimated result among these spells. ")
                 : QString()) +
            links.join(" · "));
    } else {
        for (int n = 1; n < attacks.entries().size(); ++n)
            if (AttackData::templateTargets(attacks.entry(n).behavior).values().contains(id))
                links << QString("<a href=\"%1\">%2</a>")
                             .arg(n)
                             .arg(attackName(attacks, n).toHtmlEscaped());
        relationship_->setText(links.isEmpty() ? QString()
                                               : "Resolved spell used by " + links.join(", "));
    }
    effectPanel_->setEnabled(!templ);
    if (u8(a.data, 1) != 8 && element_->findData(8) >= 0)
        element_->removeItem(element_->findData(8));
    if (u8(a.data, 1) == 8 && element_->findData(8) < 0)
        element_->addItem("Resolved by spell template", 8);
    choose(element_, u8(a.data, 1) == 255 ? 15 : u8(a.data, 1));
    choose(effect_, u8(a.data, 3));
    choose(calculation_, u8(a.data, 4));
    power_->setValue(a.powerPercent);
    const int rule=u8(a.data,4);
    power_->setEnabled(rule!=3 && rule!=4 && rule!=6 && rule!=9);
    const int effect = u8(a.data, 3);
    amountPanel_->setVisible(effect == 1 || effect == 2);
    elementHelp_->setText(
        u8(a.data, 1) == 15 || u8(a.data, 1) == 255
            ? (u8(a.data, 0) == 3
                   ? "Element comes from the first equipped spellbook; no matching book gives "
                     "Physical."
                   : "Element comes from the first equipped weapon; no weapon gives Physical.")
        : u8(a.data, 1) == 8
            ? "Existing template setting: resolves from a spellbook, weapon or the character's "
              "element. Select the resolved spell above to edit its element."
            : "Element changes resistance and elemental bonuses. It keeps the assigned animation "
              "and physical / magic calculation family.");
    const int element = u8(a.data, 1);
    if (element >= 9 && element <= 14) {
        const int reduced[] = {1, 2, 3, 4, 1, 2};
        elementHelp_->setText(QString("This combined element uses %1 resistance and bonuses. It "
                                      "keeps the assigned animation.")
                                  .arg(AttackData::elements().value(reduced[element - 9])));
    }
    const int originalPattern = nativePattern(*project_, a);
    const QString automaticSide = effect == 2 || effect == 5 || effect == 6 || effect == 11
                                      ? "Allies (including user)" : "Opponents";
    side_->setItemText(side_->findData(0), templ ? "From resolved spell"
                         : a.targetPattern || a.targetSide
                             ? automaticSide + " (from main effect)"
                             : nativeSideName(a) + " (game rule)");
    pattern_->setItemText(pattern_->findData(0), templ ? "From resolved spell"
                          : a.targetSide ? "One target (chosen side)"
                                         : nativePatternName(originalPattern) + " (game rule)");
    choose(side_, a.targetPattern == 9 ? 3 : a.targetSide);
    choose(pattern_, a.targetPattern);
    side_->setEnabled(!templ && a.targetPattern != 9);
    pattern_->setEnabled(!templ);
    int mask = a.targetPattern == 12                          ? a.targetMask
               : a.targetPattern == 2                         ? 511
               : a.targetPattern >= 3 && a.targetPattern <= 5 ? 7 << ((a.targetPattern - 3) * 3)
               : a.targetPattern == 6                         ? 146
               : a.targetPattern == 7                         ? 56
               : a.targetPattern == 8                         ? 186
                                                              : 16;
    if (!a.targetPattern && !a.targetSide)
        mask = originalPattern == 1 || originalPattern == 8                            ? 16
               : originalPattern >= 2 && originalPattern <= 4                          ? 186
               : originalPattern == 5 || originalPattern == 6 || originalPattern == 11 ? 511
               : originalPattern == 7                                                  ? 186
               : originalPattern == 9 || originalPattern == 10                        ? 511
               : originalPattern == 12                                                 ? 146
               : originalPattern == 14                                                 ? 16
                                                                                       : 0;
    if (templ)
        mask = 0;
    for (int i = 0; i < 9; ++i) {
        cells_[i]->setChecked(mask & (1 << i));
        cells_[i]->setEnabled(!templ && a.targetPattern == 12);
    }
    targetHelp_->setText(
        templ ? "Select a resolved spell above to see its targets and effects."
        : a.targetPattern == 0 && !a.targetSide
            ? nativePatternDetail(originalPattern) +
                  " The diagram assumes a center target. Selecting another pattern replaces this game rule."
        : a.targetPattern == 12 ? "Checked positions are fixed in the chosen side's formation. "
                                  "Empty cells are skipped. Front is nearest the opposing squad."
                                : "Diagram uses a center target. Row, column and cross patterns "
                                  "follow the selected target. Empty positions are skipped.");
    if (u8(a.data, 0) < 2)
        targetHelp_->setText(targetHelp_->text() +
                             " Physical attacks strike each selected target in turn, spending one "
                             "attack count for the whole pattern.");
    const auto originalStatus = nativeStatusRule(a);
    status_->setItemText(status_->findData(-1), templ ? "From resolved spell"
                                                     : originalStatus.first + " (game rule)");
    choose(status_, a.status);
    choose(condition_, a.condition);
    chance_->setValue(a.chance);
    conditionPanel_->setVisible(a.status > 0);
    status_->setEnabled(!templ);
    nativeStatus_->setText(
        templ ? "The resolved spell supplies its additional effect and activation rule."
        : a.status < 0
            ? originalStatus.second
            : "Weapon on-hit effects remain separate. Chance is rolled separately for each "
              "eligible target. Immunity and existing ailment "
              "restrictions still apply. Status strength and natural recovery use Tools → Status "
              "effects; effects without a recovery timer keep that behavior.");
    const auto originalAfter = nativeAfterRule(a);
    after_->setItemText(after_->findData(-1), templ ? "From resolved spell"
                                                   : originalAfter.first + " (game rule)");
    choose(after_, a.afterEffect);
    choose(basis_, a.afterBasis);
    afterPercent_->setValue(a.afterPercent);
    afterPanel_->setVisible(a.afterEffect > 0);
    after_->setEnabled(!templ);
    nativeAfter_->setText(
        templ ? "The resolved spell supplies its drain or recoil rule."
        : a.afterEffect < 0 ? originalAfter.second
                          : "Applied once after all targets. Drain is capped by missing HP; recoil "
                            "can defeat the attacker. Total HP lost counts the main effect only, "
                            "before additional status effects.");
    if (a.afterEffect > 0)
        nativeAfter_->setText(nativeAfter_->text() +
                              " Combined spells use their first caster. Pedra items have no acting "
                              "character, so they do not apply drain or recoil.");
    advanced_->setText(QString("D0 execution family: %1 · D2 native target selector: %2 · D5 "
                               "condition ID: %3\nD6/D7/D8/D9 native secondary IDs: %4 / %5 / %6 / "
                               "%7\nD10: %8 · D11: %9 (unidentified, preserved)\nBehavior origin: "
                               "%10 · Animation origin: %11\nRaw D0–D11: %12")
                           .arg(u8(a.data, 0))
                           .arg(u8(a.data, 2))
                           .arg(u8(a.data, 5))
                           .arg(u8(a.data, 6))
                           .arg(u8(a.data, 7))
                           .arg(u8(a.data, 8))
                           .arg(u8(a.data, 9))
                           .arg(u8(a.data, 10))
                           .arg(u8(a.data, 11))
                           .arg(a.behavior)
                           .arg(a.animation)
                           .arg(QString::fromLatin1(a.data.toHex(' '))));
    showUsers();
    comparison();
    notice_->clear();
}
void AttackEditor::publish(const AttackData &next, const QString &title) {
    if (next.json() != project_->attacks()->json())
        undo_->push(new AttackChange(project_, title, project_->attacks()->json(), next.json()));
}
void AttackEditor::edit(const QString &title, const std::function<void(Attack &)> &fn) {
    if (refreshing_ || selected() < 1)
        return;
    try {
        auto next = *project_->attacks();
        auto a = next.entry(selected());
        fn(a);
        next.set(selected(), a);
        publish(next, title);
    } catch (const std::exception &e) {
        notice_->setText(e.what());
    }
}
void AttackEditor::editEffect() {
    if (selected() < 1)
        return;
    try {
        const int id = selected();
        const auto before = project_->attacks()->entry(id);
        if (AttackData::isTemplate(before.behavior)) {
            notice_->setText("Choose a resolved spell above to edit its visual effect.");
            return;
        }
        AttackEffectEditor dialog(project_, id, previewClass_->currentData().toInt(), this);
        if (dialog.exec() != QDialog::Accepted || dialog.visual() == before.visual)
            return;
        if (project_->attacks()->entry(id) != before)
            throw Error("The attack changed while its effect editor was open");
        auto next = *project_->attacks();
        auto edited = before;
        edited.visual = dialog.visual();
        next.set(id, edited);
        publish(next, "Edit attack visual effect");
    } catch (const std::exception &e) {
        notice_->setText(e.what());
    }
}
void AttackEditor::rename() {
    if (selected() < 1)
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Rename attack");
    auto layout = new QVBoxLayout(&dialog);
    auto name = new QLineEdit(attackName(*project_->attacks(), selected()));
    layout->addWidget(name);
    auto shared = new QCheckBox("Rename all attacks sharing this name");
    shared->setVisible(project_->attacks()->sharingName(selected()).size() > 1);
    layout->addWidget(shared);
    text("Leave unchecked to give this attack an independent name.", layout);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    try {
        auto next = *project_->attacks();
        next.rename(selected(), name->text(), shared->isChecked());
        publish(next, "Rename attack");
    } catch (const std::exception &e) {
        notice_->setText(e.what());
    }
}
void AttackEditor::create(bool copy) {
    QString name;
    int starting = selected();
    if (copy) {
        bool ok = false;
        name = QInputDialog::getText(this, "Duplicate attack", "Name", QLineEdit::Normal,
                                     attackName(*project_->attacks(), selected()) + " copy", &ok);
        if (!ok)
            return;
    } else {
        QDialog dialog(this);
        dialog.setWindowTitle("Create attack");
        auto layout = new QFormLayout(&dialog);
        auto enteredName = new QLineEdit("New attack");
        enteredName->setObjectName("attackNewName");
        auto source = new QComboBox;
        source->setObjectName("attackStartingPoint");
        const auto &attacks = *project_->attacks();
        for (int id = 1; id < attacks.entries().size(); ++id)
            if (!AttackData::isTemplate(attacks.entry(id).behavior))
                source->addItem(QString("%1 · %2 — %3")
                                    .arg(id)
                                    .arg(attackName(attacks, id),
                                         AttackData::effects().value(u8(attacks.entry(id).data, 3),
                                                                     "Existing effect")),
                                id);
        source->setCurrentIndex(source->findData(5));
        layout->addRow("Name", enteredName);
        layout->addRow("Start from", source);
        layout->addRow(text("Copies the chosen attack's settings into a new ID. Its name and "
                            "effects are independent; no classes are assigned automatically."));
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        name = enteredName->text();
        starting = source->currentData().toInt();
    }
    try {
        auto next = *project_->attacks();
        const int id = next.duplicate(starting, name);
        publish(next, copy ? "Duplicate attack" : "Create attack");
        refresh();
        selectAttack(id);
    } catch (const std::exception &e) {
        notice_->setText(e.what());
    }
}
void AttackEditor::showUsers() {
    const auto &attacks = *project_->attacks();
    QByteArray signature = QByteArray::number(selected()) + '\0';
    for (const auto &record : project_->records(Table::Classes))
        signature +=
            record.name.toUtf8() + '\0' + record.bytes.mid(55, 6) + record.bytes.mid(69, 1);
    for (const auto &attack : attacks.entries())
        signature += attack.name + '\0' + char(attack.behavior);
    if (users_->property("assignmentSignature").toByteArray() == signature)
        return;
    users_->setProperty("assignmentSignature", signature);
    users_->setRowCount(0);
    for (const auto &record : project_->records(Table::Classes)) {
        if (!record.id)
            continue;
        bool used = false, indirect = false;
        for (int row = 0; row < 3; ++row) {
            const int a = classAction(*project_, record.id, row);
            if (a == selected())
                used = true;
            else if (a > 0 && a < attacks.entries().size() &&
                     AttackData::templateTargets(attacks.entry(a).behavior)
                         .values()
                         .contains(selected())) {
                used = true;
                indirect = true;
            }
        }
        if (!used)
            continue;
        const int row = users_->rowCount();
        users_->insertRow(row);
        auto item = new QTableWidgetItem(record.name + (indirect ? " (via template)" : ""));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setData(Qt::UserRole, record.id);
        users_->setItem(row, 0, item);
        for (int rank = 0; rank < 3; ++rank) {
            QMap<int, QString> names{{0, "None"}, {255, "Inherit current class"}};
            for (int n = 1; n < attacks.entries().size(); ++n)
                names[n] = QString("%1 · %2").arg(n).arg(attackName(attacks, n));
            auto action = choices(names, "attackClassAction");
            choose(action, u8(record.bytes, 55 + rank * 2));
            auto count = number("attackClassCount", 0, 255, u8(record.bytes, 56 + rank * 2));
            users_->setCellWidget(row, rank * 2 + 1, action);
            users_->setCellWidget(row, rank * 2 + 2, count);
            const auto apply = [this, id = record.id, rank, action, count] {
                if (refreshing_)
                    return;
                auto before = project_->record(Table::Classes, id).bytes, after = before;
                put8(after, 55 + rank * 2, action->currentData().toInt());
                put8(after, 56 + rank * 2, count->value());
                if (before != after)
                    undo_->push(new AttackAssignments(project_, id, before, after));
            };
            connect(action, &QComboBox::activated, this, apply);
            connect(count, &QSpinBox::editingFinished, this, apply);
        }
    }
    users_->setVisible(users_->rowCount() > 0);
    findChild<QLabel *>("attackNoUsers")->setVisible(users_->rowCount() == 0);
    if (users_->rowCount()) {
        bool present = false;
        for (int row = 0; row < users_->rowCount(); ++row)
            present |= users_->item(row, 0)->data(Qt::UserRole).toInt() ==
                       previewClass_->currentData().toInt();
        if (!present)
            previewClass_->setCurrentIndex(
                previewClass_->findData(users_->item(0, 0)->data(Qt::UserRole)));
    }
}
void AttackEditor::assignClass() {
    QDialog dialog(this);
    dialog.setWindowTitle("Assign attack to a class");
    auto layout = new QFormLayout(&dialog);
    auto classes = new QComboBox;
    for (const auto &c : project_->records(Table::Classes))
        if (c.id)
            classes->addItem(QString("%1 · %2").arg(c.id).arg(c.name), c.id);
    auto row = choices({{0, "Front"}, {1, "Middle"}, {2, "Rear"}}, "attackAssignRow");
    auto count = number("attackAssignCount", 1, 255, 1);
    layout->addRow("Class", classes);
    layout->addRow("Formation row", row);
    layout->addRow("Attack count", count);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const int id = classes->currentData().toInt(), rank = row->currentData().toInt();
    auto before = project_->record(Table::Classes, id).bytes, after = before;
    put8(after, 55 + rank * 2, selected());
    put8(after, 56 + rank * 2, count->value());
    if (before != after)
        undo_->push(new AttackAssignments(project_, id, before, after));
    previewClass_->setCurrentIndex(previewClass_->findData(id));
}
void AttackEditor::preview() {
    try {
        project_->loadCombatArtwork();
        auto dialog = new QDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle("Attack animation preview");
        auto layout = new QVBoxLayout(dialog);
        text(previewClass_->currentText() + " · " + attackName(*project_->attacks(), selected()) +
                 " · Player / base artwork",
             layout);
        auto preview = new ActorPoseEditor(project_->rom());
        preview->setBodyAnimations(project_->bodyAnimationData());
        preview->setArtwork(project_->combatArtwork()->previewEdits());
        preview->selectPrivateSequence(
            previewClass_->currentData().toInt(), 0,
            combatAnimationPose(*project_,
                                {previewClass_->currentData().toInt(), 0, selected(), false}));
        preview->showReplacementPreview();
        layout->addWidget(preview);
        dialog->resize(760, 620);
        dialog->show();
    } catch (const std::exception &e) {
        notice_->setText(e.what());
    }
}
void AttackEditor::comparison() {
    if (selected() < 1 || example_.size() != 7)
        return;
    const auto &a = project_->attacks()->entry(selected());
    if (AttackData::isTemplate(a.behavior) || !(u8(a.data, 3) == 1 || u8(a.data, 3) == 2)) {
        comparison_->setText(
            "No HP comparison for this effect. Select a damage or healing attack.");
        return;
    }
    const auto base = selected() < opened_.entries().size() ? opened_.entry(selected())
                                                            : opened_.entry(a.behavior);
    const auto estimate = [this](const Attack &a) {
        if (u8(a.data, 3) != 1 && u8(a.data, 3) != 2)
            return 0;
        float power = float(example_[0]->value()) * a.powerPercent / 100.f;
        const int rule = u8(a.data, 4);
        if (rule == 7 || rule == 8)
            power = float(double(power) * (rule == 7 ? 1.5 : 1.6));
        const int resistance = u8(a.data, 3) == 2 ? 0 : example_[3]->value();
        float amount = ((power - float(example_[1]->value())) + float(example_[2]->value())) *
                       float(100 - resistance) / 100.f;
        int n = int(amount);
        if (rule == 2)
            n /= 2;
        else if (rule == 3)
            n = example_[4]->value() / 20;
        else if (rule == 4)
            n = example_[5]->value() / 2;
        else if (rule == 5)
            n = int(power / 6.f);
        else if (rule == 9)
            n = example_[4]->value() / 2;
        else if (rule == 10)
            n = int(double(n) * 0.8);
        n = std::clamp(n, 1, 9999);
        return u8(a.data, 3) == 2
                   ? std::min(n, std::max(0, example_[6]->value() - example_[5]->value()))
                   : std::min(n, example_[5]->value());
    };
    comparison_->setText(
        QString("<b>%1: %2 HP %3 → Current: %4 HP %5</b>")
            .arg(selected() < opened_.entries().size() ? "Opened ROM" : "Starting attack")
            .arg(estimate(base))
            .arg(u8(base.data, 3) == 2 ? "restored" : "lost")
            .arg(estimate(a))
            .arg(u8(a.data, 3) == 2 ? "restored" : "lost"));
}
} // namespace lordly
