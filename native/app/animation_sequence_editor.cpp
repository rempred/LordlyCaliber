#include "app/view_position.h"
#include "app/animation_sequence_editor.h"
#include "app/actor_pose_editor.h"
#include "app/art_image_assignment.h"
#include "app/frame_layer_editor.h"
#include "app/sprite_library_command.h"
#include "engine/sprite_object.h"
#include "engine/sprite_sources.h"
#include "engine/animation_transfer.h"
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class SequenceCommand final : public QUndoCommand {
  public:
    SequenceCommand(Project *project, QString label, QJsonObject after, QJsonObject overrides)
        : QUndoCommand(label), project_(project), before_(project->animationSequences()->json()),
          after_(std::move(after)), beforeOverrides_(project->animationOverrides()->json()),
          afterOverrides_(std::move(overrides)) {}
    void undo() override {
        project_->setPrivateAnimationState(before_, beforeOverrides_);
    }
    void redo() override {
        project_->setPrivateAnimationState(after_, afterOverrides_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
    QJsonObject beforeOverrides_, afterOverrides_;
};
QString poseName(int id) {
    const QMap<int, QString> names{{0, "Idle"},
                                   {5, "Advance"},
                                   {11, "Return"},
                                   {17, "Hit"},
                                   {38, "Frame substitutions A"},
                                   {88, "Frame substitutions B"}};
    return names.contains(id) ? QString("%1 · %2").arg(id).arg(names[id])
                              : QString("Pose %1").arg(id);
}
bool isFrame(const PoseRecord &r) {
    return r.opcode == 1 || r.opcode == 21;
}
int token(const PoseRecord &r) {
    return u8(r.operands, 0) + (r.opcode == 21 ? 256 * u8(r.operands, 1) : 0);
}
} // namespace
AnimationSequenceEditor::AnimationSequenceEditor(Project *project, QUndoStack *undo,
                                                 std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("animationSequenceEditor");
    auto outer = new QVBoxLayout(this);
    auto title = new QLabel("<b>Private class sequences</b>");
    outer->addWidget(title);
    auto description = new QLabel(
        "Create an independent copy for a class and body route. Edit frame timing and order while "
        "retaining the pose's control instructions. The preview uses the current project.");
    description->setWordWrap(true);
    outer->addWidget(description);
    advanced_ << title << description;
    notice_ = new QLabel;
    notice_->setObjectName("sequenceNotice");
    notice_->setWordWrap(true);
    outer->addWidget(notice_);
    try {
        project_->loadAnimationRoutes();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        return;
    }
    rawWorkspace_ = new QWidget;
    outer->addWidget(rawWorkspace_, 1);
    auto layout = new QVBoxLayout(rawWorkspace_);
    layout->setContentsMargins(0, 0, 0, 0);
    auto choices = new QHBoxLayout;
    classes_ = new QComboBox;
    classes_->setObjectName("sequenceClass");
    classes_->setEditable(true);
    classes_->setInsertPolicy(QComboBox::NoInsert);
    classes_->completer()->setFilterMode(Qt::MatchContains);
    classes_->setMinimumContentsLength(20);
    routes_ = new QComboBox;
    routes_->setObjectName("sequenceRoute");
    routes_->addItems({"Body 0 / 0", "Body 0 / 1", "Body 1 / 0", "Body 1 / 1"});
    poses_ = new QComboBox;
    poses_->setObjectName("sequencePose");
    choices->addWidget(classes_, 2);
    choices->addWidget(routes_);
    choices->addWidget(poses_);
    layout->addLayout(choices);
    advanced_ << classes_ << routes_ << poses_;
    auto buttons = new QHBoxLayout;
    create_ = new QPushButton("Make private copy");
    create_->setObjectName("sequenceCreate");
    remove_ = new QPushButton("Remove private copy");
    remove_->setObjectName("sequenceRemoveCopy");
    buttons->addWidget(create_);
    buttons->addWidget(remove_);
    auto copy = new QPushButton("Copy pose frames to library");
    copy->setObjectName("sequenceCopyToLibrary");
    copy->setToolTip("Copy the authored frames, layers and timing into Art Library. Uses class "
                    "equipment defaults. Loops, movement, material changes and sound remain in "
                    "the game sequence.");
    buttons->addWidget(copy);
    connect(copy, &QPushButton::clicked, this, [this] { guarded([this] { copyToLibrary(); }); });
    auto importSequence = new QPushButton("Import / replace library sequence…");
    importSequence->setObjectName("sequenceImportAll");
    buttons->addWidget(importSequence);
    connect(importSequence, &QPushButton::clicked, this, [this] {
        guarded([this] {
            const auto assets = project_->sprites().assets();
            if (assets.isEmpty()) throw Error("Add a sequence in Art Library first");
            QStringList names;
            for (int i = 0; i < assets.size(); ++i)
                names << QString("%1 · %2 (%3 frames)").arg(i + 1).arg(assets[i].name).arg(assets[i].frames.size());
            bool ok = false;
            const auto choice = QInputDialog::getItem(this, "Import complete sequence", "Art Library", names, 0, false, &ok);
            if (ok) this->importSequence(assets[names.indexOf(choice)]);
        });
    });
    buttons->addStretch();
    layout->addLayout(buttons);
    auto tools = new QHBoxLayout;
    for (const auto &entry : {qMakePair("Copy / replace from class…", "sequenceCopyFrom"),
                              qMakePair("Compare sequences…", "sequenceCompare"),
                              qMakePair("Translate frames…", "sequenceTranslate")}) {
        auto button = new QPushButton(entry.first);
        button->setObjectName(entry.second);
        tools->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, button] {
            guarded([this, button] {
                if (button->objectName() == "sequenceTranslate") chooseTranslation();
                else chooseSource(button->objectName() == "sequenceCompare");
            });
        });
    }
    tools->addStretch();
    layout->addLayout(tools);
    auto split = rawSplit_ = new QSplitter;
    auto left = new QWidget;
    auto panel = new QVBoxLayout(left);
    panel->setContentsMargins(0, 0, 0, 0);
    records_ = new QTableWidget(0, 4);
    records_->setObjectName("sequenceRecords");
    records_->setHorizontalHeaderLabels({"Instruction", "Frame", "Ticks", "Control operands"});
    records_->setSelectionBehavior(QAbstractItemView::SelectRows);
    records_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    records_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    records_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    records_->horizontalHeader()->setStretchLastSection(true);
    panel->addWidget(records_, 1);
    edits_ = new QWidget;
    auto form = new QFormLayout(edits_);
    frame_ = new QSpinBox;
    frame_->setObjectName("sequenceFrame");
    ticks_ = new QSpinBox;
    ticks_->setObjectName("sequenceTicks");
    ticks_->setRange(0, 255);
    ticks_->setValue(2);
    form->addRow("Frame", frame_);
    form->addRow("Duration (game ticks)", ticks_);
    auto editButtons = new QGridLayout;
    const auto add = [&](const QString &label, const char *name, int row, int column,
                         const std::function<void()> &action) {
        auto b = new QPushButton(label);
        b->setObjectName(name);
        editButtons->addWidget(b, row, column);
        connect(b, &QPushButton::clicked, this, [this, action] { guarded(action); });
    };
    add("Apply frame", "sequenceApplyFrame", 0, 0, [this] {
        edit("Edit sequence frame", [this](PoseBank &bank, int state, int row) {
            bank.setFrame(state, row, frame_->value(), ticks_->value());
        });
    });
    add("Insert before", "sequenceInsertFrame", 0, 1, [this] {
        edit("Insert sequence frame", [this](PoseBank &bank, int state, int row) {
            bank.insertFrame(state, std::max(0, row), frame_->value(), ticks_->value());
        });
    });
    add("Duplicate frame", "sequenceDuplicateFrame", 1, 0, [this] {
        edit("Duplicate sequence frame", [](PoseBank &bank, int state, int row) {
            const auto records = bank.effective(state).records;
            if (row < 0 || row >= records.size() || !isFrame(records[row]))
                throw Error("Choose a frame instruction");
            bank.insertFrame(state, row + 1, token(records[row]),
                             u8(records[row].operands, records[row].operands.size() - 1));
        });
    });
    add("Remove frame", "sequenceRemoveFrame", 1, 1, [this] {
        edit("Remove sequence frame",
             [](PoseBank &bank, int state, int row) { bank.removeFrame(state, row); });
    });
    add("Move up", "sequenceMoveUp", 2, 0, [this] {
        edit("Move sequence frame",
             [](PoseBank &bank, int state, int row) { bank.moveFrame(state, row, row - 1); });
    });
    add("Move down", "sequenceMoveDown", 2, 1, [this] {
        edit("Move sequence frame",
             [](PoseBank &bank, int state, int row) { bank.moveFrame(state, row, row + 1); });
    });
    add("Edit frame layers", "sequenceEditLayers", 3, 0, [this] { editLayers(); });
    add("Paint frame sprite…", "sequenceEditSprite", 5, 0, [this] { editSprite(); });
    add("Copy frame art…", "sequenceCopyFrameArt", 4, 0, [this] {
        if (records_->currentRow() < 0) throw Error("Choose a destination frame instruction");
        chooseSource(false, records_->currentRow(), false);
    });
    add("Copy art and duration…", "sequenceCopyFrameTiming", 4, 1, [this] {
        if (records_->currentRow() < 0) throw Error("Choose a destination frame instruction");
        chooseSource(false, records_->currentRow(), true);
    });
    add("Import frame image…", "sequenceImport", 3, 1, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Import private sequence frame", {},
                                                       "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty())
            importImage(path);
    });
    form->addRow(editButtons);
    auto libraryRow = new QHBoxLayout;
    library_ = new QComboBox;
    library_->setObjectName("sequenceLibrary");
    libraryFrame_ = new QSpinBox;
    libraryFrame_->setObjectName("sequenceLibraryFrame");
    libraryFrame_->setRange(1, 1);
    libraryFrame_->setPrefix("Frame ");
    auto importLibrary = new QPushButton("Insert library frame…");
    importLibrary->setObjectName("sequenceImportLibrary");
    libraryRow->addWidget(library_, 1);
    libraryRow->addWidget(libraryFrame_);
    libraryRow->addWidget(importLibrary);
    form->addRow(libraryRow);
    connect(library_, &QComboBox::currentIndexChanged, this, [this] {
        libraryFrame_->setMaximum(
            library_->currentIndex() < 0
                ? 1
                : int(project_->sprites().asset(library_->currentData().toString()).frames.size()));
    });
    connect(importLibrary, &QPushButton::clicked, this, [this] {
        guarded([this] {
            if (library_->currentIndex() < 0)
                throw Error("Choose a Art Library image");
            prepareImage(project_->sprites()
                             .asset(library_->currentData().toString())
                             .composite(libraryFrame_->value() - 1));
        });
    });
    assignmentControls_ = new QWidget;
    auto assignmentForm = new QFormLayout(assignmentControls_);
    assignmentForm->setContentsMargins(0, 0, 0, 0);
    form->addRow(assignmentControls_);
    auto attackRow = new QHBoxLayout;
    actions_ = new QComboBox;
    actions_->setObjectName("sequenceAttack");
    for (const auto &value : AnimationOverrideImage::definition()["actions"].toArray()) {
        const auto action = value.toObject();
        actions_->addItem(action["name"].toString(), action["id"].toInt());
    }
    attackModes_ = new QComboBox;
    attackModes_->setObjectName("sequenceAttackMode");
    attackModes_->addItems({"Normal", "Blocked"});
    attackRow->addWidget(actions_, 1);
    attackRow->addWidget(attackModes_);
    assignmentForm->addRow("Attack", attackRow);
    auto assignmentButtons = new QHBoxLayout;
    auto assign = new QPushButton("Copy pose to attack");
    assign->setObjectName("sequenceAssignAttack");
    auto restore = new QPushButton("Restore attack assignment");
    restore->setObjectName("sequenceRestoreAttack");
    assignmentButtons->addWidget(assign);
    assignmentButtons->addWidget(restore);
    assignmentForm->addRow(assignmentButtons);
    assignmentSummary_ = new QLabel;
    assignmentSummary_->setObjectName("sequenceAssignmentSummary");
    assignmentSummary_->setWordWrap(true);
    assignmentForm->addRow(assignmentSummary_);
    connect(actions_, &QComboBox::currentIndexChanged, this, [this] { refreshAssignments(); });
    connect(assign, &QPushButton::clicked, this, [this] {
        guarded([this] {
            auto candidate = *project_;
            const int pose = candidate.assignPrivateAttack(
                selectedClass(), routes_->currentIndex(), actions_->currentData().toInt(),
                attackModes_->currentIndex() == 1, poses_->currentData().toInt());
            publish(candidate, "Create private attack sequence");
            poses_->setCurrentIndex(poses_->findData(pose));
        });
    });
    connect(restore, &QPushButton::clicked, this, [this] {
        guarded([this] {
            auto candidate = *project_;
            candidate.restorePrivateAttack(selectedClass(), routes_->currentIndex(),
                                           actions_->currentData().toInt());
            publish(candidate, "Restore attack assignment");
        });
    });
    panel->addWidget(edits_);
    summary_ = new QLabel;
    summary_->setObjectName("sequenceSummary");
    summary_->setWordWrap(true);
    panel->addWidget(summary_);
    preview_ = new ActorPoseEditor(project_->rom());
    preview_->setObjectName("sequencePreview");
    auto leftScroll = new QScrollArea;
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setWidget(left);
    split->addWidget(leftScroll);
    split->addWidget(preview_);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    layout->addWidget(split, 1);
    connect(classes_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_)
            refresh();
    });
    connect(routes_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_)
            refresh();
    });
    connect(poses_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_) {
            preferredPose_ = poses_->currentData().toInt();
            refresh();
        }
    });
    connect(records_, &QTableWidget::currentCellChanged, this, [this] {
        if (!refreshing_)
            guarded([this] { selectRecord(); });
    });
    connect(create_, &QPushButton::clicked, this, [this] {
        guarded([this] {
            project_->loadCombatArtwork();
            auto next = *project_->animationSequences();
            next.create(selectedClass(), routes_->currentIndex(), *project_->animationRoutes(),
                        project_->combatArtwork()->previewEdits());
            apply(std::move(next), "Create private animation copy");
        });
    });
    connect(remove_, &QPushButton::clicked, this, [this] {
        guarded([this] {
            auto candidate = *project_;
            candidate.removePrivateAnimation(selectedClass(), routes_->currentIndex());
            publish(candidate, "Remove private animation copy");
        });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] { refresh(); });
    refresh();
}
void AnimationSequenceEditor::setContext(int id, int flags, int pose) {
    if (!classes_) return;
    {
        const QSignalBlocker a(classes_), b(routes_);
        classes_->setCurrentIndex(classes_->findData(id));
        routes_->setCurrentIndex(flags);
        preferredPose_ = pose;
    }
    refresh();
}
void AnimationSequenceEditor::setGuided(bool value) {
    guided_ = value;
    if (value && !frameWorkspace_) buildFrameWorkspace();
    rawWorkspace_->setVisible(!value);
    if (frameWorkspace_) frameWorkspace_->setVisible(value);
    if (value && preview_->parentWidget() != framePreviewHost_)
        qobject_cast<QVBoxLayout *>(framePreviewHost_->layout())->insertWidget(1, preview_, 1);
    if (!value) rawSplit_->addWidget(preview_);
    preview_->setFrameEditing(value);
    for (auto widget : advanced_) widget->setVisible(!value);
    if (assignmentControls_) assignmentControls_->setVisible(!value);
    create_->setText(value ? "Enable frame and sprite editing" : "Make private copy");
    create_->setToolTip("Creates an independent animation set for this class, side and artwork variant. Other classes keep their artwork.");
    remove_->setText(value ? "Restore this animation set" : "Remove private copy");
    remove_->setToolTip("Restores all animations and assignments in this class, side and artwork variant. Undo can recover the edits.");
    if (value) refresh();
}
void AnimationSequenceEditor::assignAnimation(int action, bool blocked, int pose) {
    const int id = selectedClass(), flags = routes_->currentIndex();
    project_->loadCombatArtwork();
    auto candidate = *project_;
    auto sequences = *candidate.animationSequences();
    if (!sequences.find(id, flags))
        sequences.create(id, flags, *candidate.animationRoutes(), candidate.combatArtwork()->previewEdits());
    if (action < 0) {
        const QMap<int, int> fixed{{-1, 0}, {-2, 5}, {-3, 11}, {-4, 17}};
        if (!fixed.contains(action)) throw Error("Unknown combat motion");
        const auto source = *sequences.find(id, flags);
        const auto art = ActorArt::loadBody(project_->rom(), id, owner(id), flags >> 1, flags & 1,
                                          project_->bodyAnimationData()).withDescriptor(source);
        auto target = copyAnimationPose(project_->rom(), source, fixed[action], source, art, pose, true);
        if (owner(id) == 10 || owner(id) == 25 || owner(id) == 123) {
            const auto pairs = sequences.groups().value(id * 4 + flags).pairedPoses;
            const int companion = pose < 50 ? pose + 50 : pairs.value(pose, -1);
            if (companion < 0) throw Error("Choose a primary animation with a linked companion");
            target = copyAnimationPose(project_->rom(), target, fixed[action] + 50,
                                       source, art, companion, true);
        }
        sequences.setDescriptor(id, flags, target);
        candidate.setAnimationSequences(sequences.json());
    } else {
        candidate.setAnimationSequences(sequences.json());
        candidate.assignPrivateAttack(id, flags, action, blocked, pose, false);
    }
    publish(candidate, "Assign combat animation");
}
int AnimationSequenceEditor::selectedClass() const {
    if (classes_->currentIndex() < 0 ||
        classes_->currentText() != classes_->itemText(classes_->currentIndex()))
        throw Error("Choose a class from the search results");
    return classes_->currentData().toInt();
}
int AnimationSequenceEditor::owner(int classId) const {
    const int value = u8(project_->record(Table::Classes, classId).bytes, 69);
    return value ? value : classId;
}
AnimationDescriptor AnimationSequenceEditor::descriptor() const {
    const int id = selectedClass(), flags = routes_->currentIndex();
    if (const auto *value = project_->animationSequences()->find(id, flags))
        return *value;
    const auto art = ActorArt::loadBody(project_->rom(), id, owner(id), flags >> 1, flags & 1,
                                        project_->bodyAnimationData());
    return AnimationDescriptor::load(project_->rom(), art.descriptorKey(),
        project_->combatArtwork() ? project_->combatArtwork()->previewEdits().value(art.descriptorKey())
                                  : QMap<int, QByteArray>{});
}
void AnimationSequenceEditor::guarded(const std::function<void()> &action) {
    try {
        action();
        notice_->clear();
        notice_->hide();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        notice_->show();
    }
}
void AnimationSequenceEditor::apply(AnimationSequences next, const QString &label) {
    if (next.json() == project_->animationSequences()->json())
        return;
    auto candidate = *project_;
    candidate.setAnimationSequences(next.json());
    publish(candidate, label);
}
void AnimationSequenceEditor::publish(const Project &candidate, const QString &label) {
    project_->loadAnimationOverrides();
    if (candidate.animationSequences()->json() == project_->animationSequences()->json() &&
        candidate.animationOverrides()->json() == project_->animationOverrides()->json())
        return;
    undo_->push(new SequenceCommand(project_, label, candidate.animationSequences()->json(),
                                    candidate.animationOverrides()->json()));
    changed_();
}
void AnimationSequenceEditor::refreshAssignments() {
    if (!assignmentSummary_ || !classes_ || classes_->currentIndex() < 0)
        return;
    try {
        const int id = selectedClass(), flags = routes_->currentIndex(),
                  action = actions_->currentData().toInt();
        const int field = u8(project_->record(Table::Classes, id).bytes, 69),
                  owner = field ? field : id;
        const auto animations = project_->bodyAnimationData();
        const bool assigned = project_->animationSequences()
                                  ->groups()
                                  .value(id * 4 + flags)
                                  .attackBaselines.contains(QString::number(action));
        const auto poseLabel = [&](int state) {
            const int secondary = animations->routes().privatePose(id, owner, flags, state);
            return secondary >= 0 ? QString("%1 + linked pose %2").arg(state).arg(secondary)
                                  : QString::number(state);
        };
        assignmentSummary_->setText(
            QString("%1 · Normal: pose %2 · Blocked: pose %3\n"
                    "Expansion Pak required for attack assignments.")
                .arg(assigned ? "Private assignment" : "Current assignment")
                .arg(poseLabel(animations->selector(id, owner, action, 0, flags)))
                .arg(poseLabel(animations->selector(id, owner, action, 2, flags))));
    } catch (const std::exception &e) {
        assignmentSummary_->setText(QString::fromUtf8(e.what()));
    }
}
void AnimationSequenceEditor::edit(const QString &label,
                                   const std::function<void(PoseBank &, int, int)> &action) {
    const int id = selectedClass(), flags = routes_->currentIndex();
    auto next = editableSequences();
    const auto *current = next.find(id, flags);
    if (!current)
        throw Error("Make a private copy before editing a sequence");
    auto snapshot = *current;
    auto bank = snapshot.poses();
    action(bank, poses_->currentData().toInt(), records_->currentRow());
    snapshot.setPoses(bank);
    next.setDescriptor(id, flags, snapshot);
    apply(std::move(next), label);
}
void AnimationSequenceEditor::selectRecord() {
    const auto bank = descriptor().poses();
    const auto &records = bank.effective(poses_->currentData().toInt()).records;
    const int row = records_->currentRow();
    if (row < 0 || row >= records.size() || !isFrame(records[row]))
        return;
    frame_->setValue(token(records[row]));
    ticks_->setValue(u8(records[row].operands, records[row].operands.size() - 1));
    preview_->showFrame(token(records[row]));
    if (guided_) updateFrameTools();
}
AnimationSequences AnimationSequenceEditor::editableSequences() {
    auto next = *project_->animationSequences();
    if (!next.find(selectedClass(), routes_->currentIndex())) {
        if (!guided_) throw Error("Make a private copy before editing a sequence");
        project_->loadCombatArtwork();
        next.create(selectedClass(), routes_->currentIndex(), *project_->animationRoutes(),
                    project_->combatArtwork()->previewEdits());
    }
    return next;
}
void AnimationSequenceEditor::editLayers() {
    const int id = selectedClass(), flags = routes_->currentIndex(),
              state = poses_->currentData().toInt(), index = records_->currentRow();
    auto next = editableSequences();
    const auto *original = next.find(id, flags);
    if (!original)
        throw Error("Make a private copy before editing frame layers");
    auto snapshot = *original;
    auto bank = snapshot.poses();
    const auto &program = bank.effective(state);
    if (index < 0 || index >= program.records.size() || !isFrame(program.records[index]))
        throw Error("Choose a frame instruction");
    const auto record = program.records[index];
    const auto frame = snapshot.frame(token(record));
    QByteArray newLayer(16, 0);
    put16(newLayer, 6, 1);
    put16(newLayer, 8, 1);
    put16(newLayer, 12, 1024);
    put16(newLayer, 14, 1024);
    for (int i = 0; i < snapshot.frameCount(); ++i) {
        const auto source = snapshot.frame(i);
        if (u16(source, 0)) {
            newLayer = source.mid(2, 16);
            break;
        }
    }
    const auto art = ActorArt::loadBody(project_->rom(), id, owner(id), flags >> 1, flags & 1,
                                        project_->bodyAnimationData());
    const auto render = [&, art](const QByteArray &draft) {
        auto copy = snapshot;
        const int token = copy.appendFrame(draft);
        auto draftBank = bank;
        draftBank.setFrame(state, index, token, u8(record.operands, record.operands.size() - 1));
        copy.setPoses(draftBank);
        const auto updated = art.withDescriptor(copy);
        return updated.bodyFrame(project_->rom(), token,
                                 updated.defaultAppearance(project_->rom()));
    };
    const auto result =
        editFrameLayers(frame, newLayer, snapshot.members().size() - 4, render, this);
    if (!result || *result == frame)
        return;
    const int replacement = snapshot.appendFrame(*result);
    bank.setFrame(state, index, replacement, u8(record.operands, record.operands.size() - 1));
    snapshot.setPoses(bank);
    next.setDescriptor(id, flags, snapshot);
    apply(std::move(next), "Edit private frame layers");
}
void AnimationSequenceEditor::importImage(const QString &path) {
    prepareImage(loadArtworkImage(path));
}
void AnimationSequenceEditor::importSequence(const SpriteAsset &asset) {
    const int id = selectedClass(), flags = routes_->currentIndex(), state = poses_->currentData().toInt();
    const auto before = project_->animationSequences()->json();
    const auto prepared = prepareSpriteSequence(this, asset);
    if (!prepared) return;
    if (project_->animationSequences()->json() != before)
        throw Error("Private sequences changed while sequence preparation was open");
    project_->loadCombatArtwork();
    auto next = *project_->animationSequences();
    if (!next.find(id, flags))
        next.create(id, flags, *project_->animationRoutes(), project_->combatArtwork()->previewEdits());
    auto snapshot = *next.find(id, flags);
    QVector<QPair<int, int>> frames;
    for (int i = 0; i < prepared->sprites.size(); ++i) {
        const auto sprite = SpriteObject::decode(prepared->sprites[i]);
        const int art = snapshot.appendSprite(prepared->sprites[i], 0, snapshot.controls()[3].left(512));
        QByteArray row(18, 0);
        put16(row, 0, 1); put16(row, 2, quint16(art));
        put16(row, 4, quint16(prepared->origin.x())); put16(row, 6, quint16(prepared->origin.y()));
        put16(row, 8, quint16(sprite.width())); put16(row, 10, quint16(sprite.height()));
        put16(row, 14, 1024); put16(row, 16, 1024);
        frames << qMakePair(snapshot.appendFrame(row), asset.frames[i].ticks);
    }
    auto bank = snapshot.poses();
    bank.replaceFrames(state, frames);
    snapshot.setPoses(bank);
    next.setDescriptor(id, flags, snapshot);
    apply(std::move(next), "Replace complete animation frame sequence");
}
void AnimationSequenceEditor::copyToLibrary() {
    const int id = selectedClass(), flags = routes_->currentIndex(), state = poses_->currentData().toInt();
    project_->loadCombatArtwork();
    const auto art = ActorArt::loadBody(project_->rom(), id, owner(id), flags >> 1, flags & 1,
                                       project_->bodyAnimationData());
    auto asset = spriteAssetFromPose(project_->rom(), art, state,
                                     (project_->record(Table::Classes, id).name + " · " + poseName(state)).left(120),
                                     0, {}, project_->combatArtwork()->previewEdits().value(art.descriptorKey()));
    auto library = project_->sprites();
    library.replace(asset);
    undo_->push(new SpriteLibraryCommand(project_, "Copy pose frames to Art Library", std::move(library)));
    changed_();
    library_->setCurrentIndex(library_->findData(asset.id));
}
void AnimationSequenceEditor::prepareImage(const QImage &image) {
    const int id = selectedClass(), flags = routes_->currentIndex(),
              state = poses_->currentData().toInt(), index = std::max(0, records_->currentRow()),
              ticks = ticks_->value();
    auto next = editableSequences();
    const auto *original = next.find(id, flags);
    if (!original)
        throw Error("Make a private copy before importing a frame");
    auto snapshot = *original;
    const auto before = project_->animationSequences()->json();
    const auto prepared = prepareArtworkImage(
        this, image,
        {image.size(), "sequence", "Import private frame",
         "Insert a new frame before the selected instruction. Colors use the game's 5-bit RGB "
         "precision; opacity retains all 256 levels. Other frames keep their artwork.",
         false, false, true},
        [](const QImage &target, int, bool dither) {
            const auto rgba = target.convertToFormat(QImage::Format_RGBA8888);
            const auto sprite = SpriteObject::fromImage(
                {rgba.width(), rgba.height(),
                 QByteArray(reinterpret_cast<const char *>(rgba.constBits()), rgba.sizeInBytes())},
                dither);
            const auto pixels = sprite.frame(0).image;
            return ArtImageMapping{sprite.source(),
                                   QImage(reinterpret_cast<const uchar *>(pixels.rgba.constData()),
                                          pixels.width, pixels.height, pixels.width * 4,
                                          QImage::Format_RGBA8888)
                                       .copy()};
        });
    if (!prepared)
        return;
    if (project_->animationSequences()->json() != before)
        throw Error("Private sequences changed while image preparation was open");
    const auto sprite = SpriteObject::decode(*prepared);
    const int art = snapshot.appendSprite(*prepared, 0, snapshot.controls()[3].left(512));
    QByteArray row(18, 0);
    put16(row, 0, 1);
    put16(row, 2, quint16(art));
    put16(row, 8, quint16(sprite.width()));
    put16(row, 10, quint16(sprite.height()));
    put16(row, 14, 1024);
    put16(row, 16, 1024);
    auto bank = snapshot.poses();
    const auto &records = bank.effective(state).records;
    if (index < records.size() && isFrame(records[index])) {
        const auto frame = snapshot.frame(token(records[index]));
        if (u16(frame, 0)) {
            put16(row, 4, u16(frame, 4));
            put16(row, 6, u16(frame, 6));
        }
    }
    const int frame = snapshot.appendFrame(row);
    bank.insertFrame(state, index, frame, ticks);
    snapshot.setPoses(bank);
    next.setDescriptor(id, flags, snapshot);
    apply(std::move(next), "Import private sequence frame");
}
void AnimationSequenceEditor::refresh() {
    const ViewPosition keepView(this);
    if (refreshing_ || !classes_)
        return;
    QScopedValueRollback guard(refreshing_, true);
    guarded([this] {
        const auto selectedLibrary = library_->currentData().toString();
        library_->clear();
        for (const auto &asset : project_->sprites().assets())
            library_->addItem(asset.name, asset.id);
        if (const int at = library_->findData(selectedLibrary); at >= 0)
            library_->setCurrentIndex(at);
        const int selected = classes_->currentIndex() < 0 ? 2 : classes_->currentData().toInt(),
                  pose = preferredPose_,
                  row = pendingRecord_ >= 0 ? pendingRecord_ : records_->currentRow();
        classes_->clear();
        for (const auto &value : project_->records(Table::Classes))
            if (value.id > 0)
                classes_->addItem(QString("%1 · %2").arg(value.id).arg(value.name), value.id);
        classes_->setCurrentIndex(std::max(0, classes_->findData(selected)));
        const auto data = descriptor();
        const auto bank = data.poses();
        poses_->clear();
        for (int state = 0; state < bank.programs().size(); ++state)
            poses_->addItem(poseName(state), state);
        poses_->setCurrentIndex(std::max(0, poses_->findData(pose)));
        const bool owned =
            project_->animationSequences()->find(selectedClass(), routes_->currentIndex());
        create_->setEnabled(!owned);
        remove_->setEnabled(owned);
        edits_->setEnabled(owned || guided_);
        frame_->setRange(0, data.frameCount() - 1);
        const auto &records = bank.effective(poses_->currentData().toInt()).records;
        records_->setRowCount(records.size());
        for (int i = 0; i < records.size(); ++i) {
            const auto &r = records[i];
            const bool frame = isFrame(r);
            const QStringList values{
                frame           ? "Frame"
                : r.opcode == 0 ? "End"
                                : QString("Control %1").arg(r.opcode),
                frame ? QString::number(token(r)) : QString{},
                frame ? QString::number(u8(r.operands, r.operands.size() - 1)) : QString{},
                frame ? QString{} : QString::fromLatin1(r.operands.toHex(' '))};
            for (int col = 0; col < 4; ++col)
                records_->setItem(i, col, new QTableWidgetItem(values[col]));
        }
        records_->selectRow(std::clamp(row, 0, std::max(0, int(records.size()) - 1)));
        summary_->setText(QString("%1 · %2 pose instructions · %3 available frames")
                              .arg(owned ? "Private copy" : "Shared original")
                              .arg(records.size())
                              .arg(data.frameCount()));
        if (poses_->currentData().toInt() == 38 || poses_->currentData().toInt() == 88)
            summary_->setText(
                summary_->text() +
                "\nFrame substitution: the last frame token displays the preceding "
                "frame's artwork with a warm tint. This affects that token in all poses.");
        preview_->setBodyAnimations(project_->bodyAnimationData());
        if (project_->combatArtwork())
            preview_->setArtwork(project_->combatArtwork()->previewEdits());
        preview_->selectPrivateSequence(selectedClass(), routes_->currentIndex(),
                                        poses_->currentData().toInt());
        if (guided_) refreshFrames();
        selectRecord();
        refreshAssignments();
    });
}
} // namespace lordly
