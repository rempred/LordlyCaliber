#include "app/animation_sequence_editor.h"
#include "app/actor_pose_editor.h"
#include "app/frame_layer_editor.h"
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScopedValueRollback>
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
    SequenceCommand(Project *project, QString label, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(project->animationSequences()->json()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setAnimationSequences(before_);
    }
    void redo() override {
        project_->setAnimationSequences(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
QString poseName(int id) {
    const QMap<int, QString> names{{0, "Idle"}, {5, "Advance"}, {11, "Return"}, {17, "Hit"}};
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
    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("<b>Private class sequences</b>"));
    auto description = new QLabel(
        "Create an independent copy for a class and body route. Edit frame timing and order while "
        "retaining the pose's control instructions. The preview uses the current project.");
    description->setWordWrap(true);
    layout->addWidget(description);
    notice_ = new QLabel;
    notice_->setObjectName("sequenceNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    try {
        project_->loadAnimationRoutes();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        return;
    }
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
    auto buttons = new QHBoxLayout;
    create_ = new QPushButton("Make private copy");
    create_->setObjectName("sequenceCreate");
    remove_ = new QPushButton("Remove private copy");
    remove_->setObjectName("sequenceRemoveCopy");
    buttons->addWidget(create_);
    buttons->addWidget(remove_);
    buttons->addStretch();
    layout->addLayout(buttons);
    auto split = new QSplitter;
    auto left = new QWidget;
    auto panel = new QVBoxLayout(left);
    panel->setContentsMargins(0, 0, 0, 0);
    records_ = new QTableWidget(0, 4);
    records_->setObjectName("sequenceRecords");
    records_->setHorizontalHeaderLabels({"Instruction", "Frame", "Ticks", "Control operands"});
    records_->setSelectionBehavior(QAbstractItemView::SelectRows);
    records_->setSelectionMode(QAbstractItemView::SingleSelection);
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
    form->addRow(editButtons);
    panel->addWidget(edits_);
    summary_ = new QLabel;
    summary_->setObjectName("sequenceSummary");
    summary_->setWordWrap(true);
    panel->addWidget(summary_);
    preview_ = new ActorPoseEditor(project_->rom());
    preview_->setObjectName("sequencePreview");
    split->addWidget(left);
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
        if (!refreshing_)
            refresh();
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
            auto next = *project_->animationSequences();
            next.remove(selectedClass(), routes_->currentIndex());
            apply(std::move(next), "Remove private animation copy");
        });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] { refresh(); });
    refresh();
}
int AnimationSequenceEditor::selectedClass() const {
    if (classes_->currentIndex() < 0 ||
        classes_->currentText() != classes_->itemText(classes_->currentIndex()))
        throw Error("Choose a class from the search results");
    return classes_->currentData().toInt();
}
AnimationDescriptor AnimationSequenceEditor::descriptor() const {
    const int id = selectedClass(), flags = routes_->currentIndex();
    if (const auto *value = project_->animationSequences()->find(id, flags))
        return *value;
    const auto art = ActorArt::loadBody(project_->rom(), id, id, flags >> 1, flags & 1,
                                        project_->bodyAnimationData());
    return AnimationDescriptor::load(project_->rom(), art.descriptorKey());
}
void AnimationSequenceEditor::guarded(const std::function<void()> &action) {
    try {
        action();
        notice_->clear();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void AnimationSequenceEditor::apply(AnimationSequences next, const QString &label) {
    if (next.json() == project_->animationSequences()->json())
        return;
    auto candidate = *project_;
    candidate.setAnimationSequences(next.json());
    undo_->push(new SequenceCommand(project_, label, next.json()));
    changed_();
}
void AnimationSequenceEditor::edit(const QString &label,
                                   const std::function<void(PoseBank &, int, int)> &action) {
    const int id = selectedClass(), flags = routes_->currentIndex();
    auto next = *project_->animationSequences();
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
}
void AnimationSequenceEditor::editLayers() {
    const int id = selectedClass(), flags = routes_->currentIndex(),
              state = poses_->currentData().toInt(), index = records_->currentRow();
    const auto *original = project_->animationSequences()->find(id, flags);
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
    const auto art = ActorArt::loadBody(project_->rom(), id, id, flags >> 1, flags & 1,
                                        project_->bodyAnimationData());
    const auto render = [&, art](const QByteArray &draft) {
        auto copy = snapshot;
        const int token = copy.appendFrame(draft);
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
    auto next = *project_->animationSequences();
    next.setDescriptor(id, flags, snapshot);
    apply(std::move(next), "Edit private frame layers");
}
void AnimationSequenceEditor::refresh() {
    if (refreshing_ || !classes_)
        return;
    QScopedValueRollback guard(refreshing_, true);
    guarded([this] {
        const int selected = classes_->currentIndex() < 0 ? 2 : classes_->currentData().toInt(),
                  pose = poses_->currentIndex() < 0 ? 40 : poses_->currentData().toInt(),
                  row = records_->currentRow();
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
        edits_->setEnabled(owned);
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
        selectRecord();
        summary_->setText(QString("%1 · %2 pose instructions · %3 available frames")
                              .arg(owned ? "Private copy" : "Shared original")
                              .arg(records.size())
                              .arg(data.frameCount()));
        preview_->setBodyAnimations(project_->bodyAnimationData());
        if (project_->combatArtwork())
            preview_->setArtwork(project_->combatArtwork()->previewEdits());
        preview_->selectPrivateSequence(selectedClass(), routes_->currentIndex(),
                                        poses_->currentData().toInt());
    });
}
} // namespace lordly
