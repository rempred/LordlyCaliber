#include "app/combat_animation_editor.h"
#include "app/animation_sequence_editor.h"
#include "engine/actor_art.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QGridLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
const QMap<int, int> motions{{-1, 0}, {-2, 5}, {-3, 11}, {-4, 17}};
const QMap<int, QString> motionNames{{-1, "Idle"}, {-2, "Advance"}, {-3, "Return"}, {-4, "Hit / recoil"}};
QMap<int, QString> actionNames() {
    QMap<int, QString> result;
    for (const auto &entry : AnimationOverrideImage::definition()["actions"].toArray()) {
        const auto row = entry.toObject();
        result[row["id"].toInt()] = row["name"].toString();
    }
    return result;
}
}
CombatAnimationEditor::CombatAnimationEditor(Project *project, QUndoStack *undo,
                                               std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project) {
    setObjectName("combatAnimationEditor");
    auto layout = new QVBoxLayout(this);
    auto description = new QLabel("Choose a class and combat action, then preview an animation. "
        "Assign applies it to the selected side, mode and artwork variant. Frame and sprite edits use an independent class copy.");
    description->setWordWrap(true); layout->addWidget(description);
    auto filters = new QGridLayout;
    const auto combo = [&](const char *name, const QString &label, int column) {
        auto value = new QComboBox; value->setObjectName(name);
        value->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        value->setMinimumContentsLength(column == 0 ? 20 : 12);
        filters->addWidget(new QLabel(label), 0, column); filters->addWidget(value, 1, column);
        return value;
    };
    classes_ = combo("combatAnimationClass", "Class", 0);
    classes_->setEditable(true); classes_->setInsertPolicy(QComboBox::NoInsert);
    classes_->completer()->setFilterMode(Qt::MatchContains);
    actions_ = combo("combatAnimationAction", "Action", 1);
    side_ = combo("combatAnimationSide", "Side", 2); side_->addItems({"Player", "Enemy"});
    mode_ = combo("combatAnimationMode", "Mode", 3);
    mode_->addItems({"Normal (modes 0 / 1)", "Blocked (mode 2)"});
    mode_->setToolTip("Normal assignments apply to both game modes 0 and 1, preserving the existing animation patch behavior.");
    variant_ = combo("combatAnimationVariant", "Artwork variant", 4);
    variant_->addItems({"Base", "Alternate"});
    variant_->setToolTip("The game has two artwork variants per side. Each can have its own animation set.");
    layout->addLayout(filters);
    auto selection = new QHBoxLayout;
    selection->addWidget(new QLabel("Animation"));
    animations_ = new QComboBox; animations_->setObjectName("combatAnimationChoice");
    animations_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    animations_->setMinimumContentsLength(24); selection->addWidget(animations_, 1);
    assign_ = new QPushButton("Assign"); assign_->setObjectName("combatAnimationAssign");
    selection->addWidget(assign_);
    auto current = new QPushButton("Show assigned"); current->setObjectName("combatAnimationShowAssigned");
    selection->addWidget(current); layout->addLayout(selection);
    assignment_ = new QLabel; assignment_->setObjectName("combatAnimationAssignment");
    assignment_->setWordWrap(true); layout->addWidget(assignment_);
    notice_ = new QLabel; notice_->setObjectName("combatAnimationNotice"); notice_->setWordWrap(true);
    layout->addWidget(notice_);
    try {
        project_->loadAnimationRoutes(); project_->loadAnimationOverrides(); project_->loadCombatArtwork();
        sequence_ = new AnimationSequenceEditor(project_, undo, std::move(changed));
        sequence_->setGuided(true); layout->addWidget(sequence_, 1);
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what())); setEnabled(false); return;
    }
    for (auto value : {classes_, actions_, side_, mode_, variant_})
        connect(value, &QComboBox::currentIndexChanged, this, [this] { if (!refreshing_) refresh(true); });
    connect(classes_, &QComboBox::editTextChanged, this, [this] {
        if (refreshing_) return;
        const bool chosen = classes_->currentIndex() >= 0 && classes_->currentText() == classes_->itemText(classes_->currentIndex());
        sequence_->setEnabled(chosen);
        if (!chosen) assign_->setEnabled(false);
        else preview();
    });
    connect(animations_, &QComboBox::currentIndexChanged, this, [this] { if (!refreshing_) preview(); });
    connect(current, &QPushButton::clicked, this, [this] { refresh(true); });
    connect(assign_, &QPushButton::clicked, this, [this] {
        try {
            sequence_->setContext(classId(), flags(), animations_->currentData().toInt());
            sequence_->assignAnimation(actions_->currentData().toInt(), mode_->currentIndex() == 1,
                                       animations_->currentData().toInt());
            refresh(true);
        } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); notice_->show(); }
    });
    connect(undo, &QUndoStack::indexChanged, this, [this] { refresh(); });
    refresh(true);
}
int CombatAnimationEditor::classId() const {
    if (classes_->currentIndex() < 0 || classes_->currentText() != classes_->itemText(classes_->currentIndex()))
        throw Error("Choose a class from the search results");
    return classes_->currentData().toInt();
}
int CombatAnimationEditor::owner() const {
    const int id = classId(), copy = u8(project_->record(Table::Classes, id).bytes, 69);
    return copy ? copy : id;
}
int CombatAnimationEditor::flags() const {
    // Valkyrie and Freya use the opposite artwork variant on the enemy side.
    const bool reversed = side_->currentIndex() == 1 && (owner() == 15 || owner() == 30);
    return ((variant_->currentIndex() ^ int(reversed)) << 1) | side_->currentIndex();
}
int CombatAnimationEditor::assignedPose(int mode) const {
    const int action = actions_->currentData().toInt();
    return action < 0 ? motions.value(action)
                      : project_->bodyAnimationData()->selector(classId(), owner(), action, mode, flags());
}
void CombatAnimationEditor::refresh(bool assigned) {
    if (refreshing_ || !sequence_) return;
    QScopedValueRollback guard(refreshing_, true);
    sequence_->setEnabled(false);
    try {
        notice_->clear(); notice_->hide();
        const int id = classes_->currentIndex() < 0 ? 2 : classId();
        const int previousAction = actions_->currentIndex() < 0 ? -999 : actions_->currentData().toInt();
        const int previousPose = animations_->currentData().toInt();
        classes_->clear();
        for (const auto &row : project_->records(Table::Classes))
            if (row.id) classes_->addItem(QString("%1 · %2").arg(row.id).arg(row.name), row.id);
        classes_->setCurrentIndex(std::max(0, classes_->findData(id)));
        actions_->clear();
        const auto names = actionNames();
        const auto record = project_->record(Table::Classes, classId()).bytes;
        const auto base = project_->record(Table::Classes, owner()).bytes;
        QMap<int, QStringList> used;
        const QStringList rows{"Front", "Middle", "Rear"};
        for (int row = 0; row < 3; ++row) {
            int action = u8(record, 55 + row * 2);
            if (action == 255) action = u8(base, 55 + row * 2);
            if (action != 255) used[action] << rows[row];
        }
        for (auto it = used.cbegin(); it != used.cend(); ++it)
            actions_->addItem(names.value(it.key(), QString("Action %1").arg(it.key())) + " · " + it->join(" / "), it.key());
        for (int action : {-1, -2, -3, -4}) actions_->addItem(motionNames[action], action);
        actions_->setCurrentIndex(std::max(0, actions_->findData(previousAction)));
        const int action = actions_->currentData().toInt(), route = flags();
        mode_->setEnabled(action >= 0);
        const int active = assignedPose(mode_->currentIndex() == 1 ? 2 : 0);
        const auto art = ActorArt::loadBody(project_->rom(), classId(), owner(), route >> 1, route & 1, project_->bodyAnimationData());
        const auto privateData = project_->animationSequences()->find(classId(), route);
        const auto descriptor = privateData ? *privateData : AnimationDescriptor::load(project_->rom(), art.descriptorKey(), project_->combatArtwork()->previewEdits().value(art.descriptorKey()));
        const auto bank = descriptor.poses();
        QMap<int, QStringList> labels;
        for (auto it = motions.cbegin(); it != motions.cend(); ++it) labels[it.value()] << motionNames[it.key()];
        auto selectors = project_->animationRoutes()->entry(classId()).selectors;
        if (u8(record, 69) != owner() || selectors.isEmpty()) selectors = project_->animationRoutes()->entry(owner()).selectors;
        const auto body = project_->bodyAnimationData();
        for (int at = 0; at + 3 < selectors.size(); at += 4) {
            const int key = u8(selectors, at);
            for (int mode = 0; mode < 3; ++mode) {
                const int pose = body->selector(classId(), owner(), key, mode, route);
                const auto label = names.value(key, QString("Action %1").arg(key)) + (mode == 2 ? " (blocked)" : "");
                if (!labels[pose].contains(label)) labels[pose] << label;
            }
        }
        animations_->clear();
        for (int pose = 0; pose < bank.programs().size(); ++pose) {
            if (pose == 38 || pose == 88) continue; // Frame substitution tables, not playable actions.
            try { bank.validateEntry(pose); } catch (const Error &) { continue; }
            auto label = QString("Animation %1").arg(pose);
            if (!labels[pose].isEmpty()) label += " · " + labels[pose].join(", ");
            if (pose == active) label += " — assigned";
            animations_->addItem(label, pose);
        }
        int selection = animations_->findData(assigned ? active : previousPose);
        if (selection < 0) selection = animations_->findData(active);
        animations_->setCurrentIndex(selection);
        auto summary = QString("Assigned: animation %1 · %2 · %3").arg(active).arg(side_->currentText(), variant_->currentText());
        if (action >= 0 && mode_->currentIndex() == 0 && assignedPose(1) != active)
            summary += QString(". Original mode 1 uses animation %1; Assign changes both normal modes.").arg(assignedPose(1));
        if (action >= 0) summary += "  Attack assignment exports use the existing Expansion Pak patch.";
        assignment_->setText(summary);
        assign_->setEnabled(selection >= 0 && (active != animations_->currentData().toInt() || (action >= 0 && mode_->currentIndex() == 0 && assignedPose(1) != active)));
        preview();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what())); notice_->show(); assign_->setEnabled(false);
    }
}
void CombatAnimationEditor::preview() {
    try {
        if (animations_->currentIndex() < 0) return;
        notice_->clear(); notice_->hide();
        const int pose = animations_->currentData().toInt();
        sequence_->setContext(classId(), flags(), pose);
        sequence_->setEnabled(true);
        const int active = assignedPose(mode_->currentIndex() == 1 ? 2 : 0);
        assign_->setEnabled(pose != active || (actions_->currentData().toInt() >= 0 && mode_->currentIndex() == 0 && assignedPose(1) != active));
        assign_->setToolTip({});
        const bool linked = owner() == 10 || owner() == 25 || owner() == 123;
        const auto pairs = project_->animationSequences()->groups().value(classId() * 4 + flags()).pairedPoses;
        if (linked && pose >= 50 && pairs.value(pose, -1) < 256) {
            assign_->setEnabled(false);
            assign_->setToolTip("This is a linked companion animation. Assign its primary animation to keep both parts together.");
        } else if (pose > 255 && actions_->currentData().toInt() >= 0) {
            assign_->setEnabled(false);
            assign_->setToolTip("Attack assignments use animation IDs 0–255. This animation can still be previewed and edited.");
        }
    } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); notice_->show(); assign_->setEnabled(false); sequence_->setEnabled(false); }
}
} // namespace lordly
