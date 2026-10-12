#include "app/actor_pose_editor.h"
#include "app/animation_sequence_editor.h"
#include "engine/sprite_sources.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QDropEvent>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>

namespace lordly {
namespace {
int frameToken(const PoseRecord &r) {
    return u8(r.operands, 0) + (r.opcode == 21 ? 256 * u8(r.operands, 1) : 0);
}
bool visibleFrame(const PoseRecord &r) {
    return r.opcode == 1 || r.opcode == 21;
}
class FrameDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyledItemDelegate::paint(p, option, index);
        if (index.data(Qt::UserRole + 1).toBool()) {
            p->save();
            p->setPen(QPen(QColor("#62bd91"), 3));
            p->drawRoundedRect(option.rect.adjusted(2, 2, -2, -2), 4, 4);
            p->restore();
        }
    }
};
class FrameBoard final : public QListWidget {
  public:
    using QListWidget::QListWidget;
    std::function<void(int, int)> move;

  protected:
    void startDrag(Qt::DropActions) override {
        if (selectedItems().size() != 1)
            return;
        QDrag drag(this);
        drag.setMimeData(model()->mimeData(selectedIndexes()));
        drag.setPixmap(selectedItems().first()->icon().pixmap(iconSize()));
        // The project command owns the move. The base implementation can remove
        // source items after exec(), which would desynchronize the rebuilt board.
        drag.exec(Qt::MoveAction);
    }
    void dropEvent(QDropEvent *event) override {
        if (event->source() != this || selectedItems().size() != 1) {
            event->ignore();
            return;
        }
        const int from = row(selectedItems().first());
        auto target = itemAt(event->position().toPoint());
        int insertion = target ? row(target) : count();
        if (target && event->position().x() > visualItemRect(target).center().x())
            ++insertion;
        const int to = std::clamp(insertion - int(from < insertion), 0, count() - 1);
        event->setDropAction(Qt::MoveAction);
        event->accept();
        // Do not let Qt reorder its model independently of the undoable game edit.
        if (move && from != to)
            move(from, to);
    }
};
} // namespace
void AnimationSequenceEditor::buildFrameWorkspace() {
    frameWorkspace_ = new QWidget;
    frameWorkspace_->setObjectName("sequenceFrameWorkspace");
    qobject_cast<QVBoxLayout *>(layout())->addWidget(frameWorkspace_, 1);
    auto layout = new QVBoxLayout(frameWorkspace_);
    layout->setContentsMargins(0, 0, 0, 0);
    auto split = new QSplitter;
    split->setObjectName("sequenceFrameSplit");
    split->setChildrenCollapsible(false);
    layout->addWidget(split, 1);
    auto left = new QWidget;
    left->setMinimumWidth(360);
    auto column = new QVBoxLayout(left);
    column->setContentsMargins(0, 0, 0, 0);
    auto heading = new QHBoxLayout;
    frameHeading_ = new QLabel("Frames");
    frameHeading_->setObjectName("sequenceFrameHeading");
    heading->addWidget(frameHeading_, 1);
    auto tools = new QPushButton("Animation tools");
    tools->setObjectName("sequenceAnimationTools");
    auto menu = new QMenu(tools);
    const auto legacyAction = [&](const QString &label, const char *name) {
        auto action = menu->addAction(label);
        connect(action, &QAction::triggered, this, [this, name] {
            if (auto button = findChild<QPushButton *>(name))
                button->click();
        });
        return action;
    };
    legacyAction("Copy animation from another class…", "sequenceCopyFrom");
    legacyAction("Compare with another animation…", "sequenceCompare");
    menu->addSeparator();
    legacyAction("Save frames to Art Library", "sequenceCopyToLibrary");
    legacyAction("Replace frames from Art Library…", "sequenceImportAll");
    menu->addSeparator();
    auto restore =
        legacyAction("Restore all animations for this side and variant", "sequenceRemoveCopy");
    connect(menu, &QMenu::aboutToShow, this,
            [this, restore] { restore->setEnabled(remove_->isEnabled()); });
    tools->setMenu(menu);
    heading->addWidget(tools);
    for (bool redo : {false, true}) {
        auto button = new QPushButton(redo ? "Redo" : "Undo");
        button->setObjectName(redo ? "sequenceRedo" : "sequenceUndo");
        button->setEnabled(redo ? undo_->canRedo() : undo_->canUndo());
        connect(button, &QPushButton::clicked, undo_, redo ? &QUndoStack::redo : &QUndoStack::undo);
        if (redo)
            connect(undo_, &QUndoStack::canRedoChanged, button, &QPushButton::setEnabled);
        else
            connect(undo_, &QUndoStack::canUndoChanged, button, &QPushButton::setEnabled);
        heading->addWidget(button);
    }
    column->addLayout(heading);
    auto hint =
        new QLabel("Click a frame to edit. Drag one to reorder. Ctrl / Shift selects several.");
    hint->setWordWrap(true);
    column->addWidget(hint);
    auto board = new FrameBoard;
    frames_ = board;
    frames_->setObjectName("sequenceFrames");
    frames_->setViewMode(QListView::IconMode);
    frames_->setFlow(QListView::LeftToRight);
    frames_->setWrapping(true);
    frames_->setResizeMode(QListView::Adjust);
    frames_->setMovement(QListView::Static);
    frames_->setSpacing(5);
    frames_->setIconSize({104, 100});
    frames_->setGridSize({124, 145});
    frames_->setUniformItemSizes(true);
    frames_->setWordWrap(false);
    frames_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    frames_->setDragDropMode(QAbstractItemView::InternalMove);
    frames_->setDefaultDropAction(Qt::MoveAction);
    frames_->setDropIndicatorShown(true);
    frames_->setItemDelegate(new FrameDelegate(frames_));
    frames_->setMinimumHeight(155);
    board->move = [this](int from, int to) { guarded([&] { moveVisibleFrame(from, to); }); };
    column->addWidget(frames_, 1);
    connect(frames_, &QListWidget::currentRowChanged, this, [this] { selectVisibleFrames(); });
    connect(frames_, &QListWidget::itemSelectionChanged, this, [this] { selectVisibleFrames(); });
    connect(frames_, &QListWidget::itemDoubleClicked, this,
            [this] { guarded([this] { editSprite(); }); });
    frameTools_ = new QWidget;
    auto toolbar = new QGridLayout(frameTools_);
    toolbar->setContentsMargins(0, 0, 0, 0);
    frameSelection_ = new QLabel;
    frameSelection_->setObjectName("sequenceFrameSelection");
    toolbar->addWidget(frameSelection_, 0, 0, 1, 2);
    duration_ = new QSpinBox;
    duration_->setObjectName("sequenceDuration");
    duration_->setRange(0, 255);
    duration_->setSuffix(" ticks");
    duration_->setKeyboardTracking(false);
    connect(duration_->findChild<QLineEdit *>(), &QLineEdit::textEdited, this,
            [this] { durationTyped_ = true; });
    duration_->setToolTip("How long each selected frame is held. Higher values make it slower. "
                          "Changes apply immediately.");
    toolbar->addWidget(new QLabel("Duration"), 0, 2);
    toolbar->addWidget(duration_, 0, 3);
    connect(duration_, &QSpinBox::valueChanged, this, [this](int value) {
        if (!refreshing_) {
            guarded([&] { changeFrameDuration(value); });
            updateFrameTools();
        }
    });
    connect(duration_, &QSpinBox::editingFinished, this, [this] {
        if (!refreshing_ && durationTyped_) {
            guarded([this] { changeFrameDuration(duration_->value()); });
            updateFrameTools();
        }
    });
    const auto button = [&](const QString &label, const char *name, int row, int col,
                            const std::function<void()> &action) {
        auto b = new QPushButton(label);
        b->setObjectName(name);
        toolbar->addWidget(b, row, col);
        connect(b, &QPushButton::clicked, this, [this, action] { guarded(action); });
        return b;
    };
    paint_ = button("Paint…", "sequencePaint", 1, 0, [this] { editSprite(); });
    layersButton_ = button("Layers…", "sequenceLayers", 1, 1, [this] { editLayers(); });
    button("Duplicate", "sequenceDuplicate", 1, 2, [this] { duplicateVisibleFrames(); });
    button("Delete", "sequenceDelete", 1, 3, [this] { removeVisibleFrames(); });
    earlier_ = button("← Earlier", "sequenceEarlier", 2, 0, [this] {
        moveVisibleFrame(frames_->currentRow(), frames_->currentRow() - 1);
    });
    later_ = button("Later →", "sequenceLater", 2, 1,
                    [this] { moveVisibleFrame(frames_->currentRow(), frames_->currentRow() + 1); });
    auto add = button("Add frame…", "sequenceAdd", 2, 2,
                      [this] { findChild<QPushButton *>("sequenceImport")->click(); });
    add->setToolTip("Import an image as a new frame before the selected frame. Use Duplicate to "
                    "start from existing artwork.");
    auto copy = button("Copy artwork…", "sequenceCopyArtwork", 2, 3,
                       [this] { chooseSource(false, records_->currentRow(), false); });
    copy->setToolTip(
        "Replace the selected frame's artwork from another class while keeping its duration.");
    column->addWidget(frameTools_);
    for (const auto &entry : {qMakePair(QKeySequence(Qt::Key_Delete), "sequenceDelete"),
                              qMakePair(QKeySequence(Qt::CTRL | Qt::Key_D), "sequenceDuplicate")}) {
        auto shortcut = new QShortcut(entry.first, frames_);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, this,
                [this, name = entry.second] { findChild<QPushButton *>(name)->click(); });
    }
    connect(preview_, &ActorPoseEditor::frameArtworkMoved, this,
            [this](QPoint offset) { guarded([&] { moveFrameArtwork(offset); }); });
    frameStatus_ = new QLabel;
    frameStatus_->setWordWrap(true);
    frameStatus_->setObjectName("sequenceFrameStatus");
    column->addWidget(frameStatus_);
    split->addWidget(left);
    auto right = framePreviewHost_ = new QWidget;
    right->setMinimumWidth(340);
    auto previewLayout = new QVBoxLayout(right);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addWidget(new QLabel("Preview"));
    previewLayout->addWidget(preview_, 1);
    auto options = new QHBoxLayout;
    onion_ = new QCheckBox("Previous frame overlay");
    onion_->setObjectName("sequenceOnionSkin");
    onion_->setToolTip(
        "Show the preceding frame faintly behind the selected artwork to compare its position.");
    options->addWidget(onion_, 1);
    auto speed = new QComboBox;
    speed->setObjectName("sequencePlaybackSpeed");
    for (double rate : {.25, .5, 1., 2.})
        speed->addItem(QString("%1× speed").arg(rate), rate);
    speed->setCurrentIndex(2);
    options->addWidget(speed);
    connect(speed, &QComboBox::currentIndexChanged, this,
            [this, speed] { preview_->setPlaybackRate(speed->currentData().toDouble()); });
    connect(onion_, &QCheckBox::toggled, this, [this] { updateFrameTools(); });
    previewLayout->addLayout(options);
    auto note = new QLabel("Drag the preview artwork to move it in the selected frames. "
                          "Drag the background or use the middle mouse button to pan. "
                          "Scroll to zoom; Escape cancels a move.");
    note->setWordWrap(true);
    previewLayout->addWidget(note);
    split->addWidget(right);
    split->setSizes({650, 450});
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    connect(preview_, &ActorPoseEditor::framePresented, this,
            [this](int state, int record, int token, bool playback) {
                if (!frames_ || !guided_)
                    return;
                for (int i = 0; i < frames_->count(); ++i) {
                    auto item = frames_->item(i);
                    const bool active = playback && state == poses_->currentData().toInt() &&
                                        item->data(Qt::UserRole).toInt() == record &&
                                        item->data(Qt::UserRole + 2).toInt() == token;
                    if (item->data(Qt::UserRole + 1).toBool() != active)
                        item->setData(Qt::UserRole + 1, active);
                }
            });
}
QSet<int> AnimationSequenceEditor::selectedFrameRecords() const {
    QSet<int> result;
    if (frames_)
        for (auto item : frames_->selectedItems())
            result.insert(item->data(Qt::UserRole).toInt());
    return result;
}
void AnimationSequenceEditor::refreshFrames() {
    if (!frames_)
        return;
    const QString context = QString("%1:%2:%3")
                                .arg(selectedClass())
                                .arg(routes_->currentIndex())
                                .arg(poses_->currentData().toInt());
    auto selection =
        context == frameContext_ && pendingRecord_ < 0 ? selectedFrameRecords() : QSet<int>{};
    const int desired =
        context == frameContext_ || pendingRecord_ >= 0 ? records_->currentRow() : -1;
    frameContext_ = context;
    pendingRecord_ = -1;
    const QSignalBlocker blocker(frames_);
    frames_->clear();
    const auto snapshot = descriptor();
    const auto bank = snapshot.poses();
    const auto records = bank.effective(poses_->currentData().toInt()).records;
    int total = 0, controls = 0;
    // A common origin and scale make relative motion visible across all cards.
    std::optional<SpriteAsset> asset;
    QString thumbnailError;
    try {
        asset = preview_->poseAsset();
    } catch (const std::exception &e) {
        thumbnailError = QString::fromUtf8(e.what());
    }
    int current = -1;
    for (int r = 0; r < records.size(); ++r) {
        if (!visibleFrame(records[r])) {
            ++controls;
            continue;
        }
        const int ticks = u8(records[r].operands, records[r].operands.size() - 1),
                  index = frames_->count();
        total += ticks;
        QPixmap thumb(104, 100);
        thumb.fill(QColor("#292e36"));
        if (asset && index < asset->frames.size()) {
            QPainter painter(&thumb);
            auto image =
                asset->composite(index).scaled(96, 92, Qt::KeepAspectRatio, Qt::FastTransformation);
            painter.drawImage(QPoint((104 - image.width()) / 2, (100 - image.height()) / 2), image);
        }
        QIcon icon(thumb);
        icon.addPixmap(thumb, QIcon::Selected); // Selection must not recolor the artwork.
        auto item = new QListWidgetItem(
            icon, QString("Frame %1\n%2 ticks").arg(index + 1).arg(ticks), frames_);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setData(Qt::UserRole, r);
        item->setData(Qt::UserRole + 2, frameToken(records[r]));
        item->setToolTip(QString("Frame %1 · %2 ticks\nDouble-click to paint.%3")
                             .arg(index + 1)
                             .arg(ticks)
                             .arg(thumbnailError.isEmpty()
                                      ? QString{}
                                      : "\nPreview unavailable: " + thumbnailError));
        if (r == desired)
            current = index;
        item->setSelected(selection.contains(r));
    }
    if (frames_->count()) {
        if (current < 0)
            current = 0;
        frames_->setCurrentRow(current, QItemSelectionModel::NoUpdate);
        if (frames_->selectedItems().isEmpty())
            frames_->item(current)->setSelected(true);
        const QSignalBlocker tableBlock(records_);
        records_->setCurrentCell(frames_->item(current)->data(Qt::UserRole).toInt(), 0);
    }
    frameHeading_->setText(QString("%1 frames · %2 ticks").arg(frames_->count()).arg(total));
    frameStatus_->setText(thumbnailError.isEmpty()
                              ? QString("Edits apply immediately with Undo. %1")
                                    .arg(project_->animationSequences()->find(
                                             selectedClass(), routes_->currentIndex())
                                             ? "This class has its own editable copy."
                                             : "An independent copy is created on your first edit.")
                              : "Some frame previews could not be drawn: " + thumbnailError);
    frameStatus_->setToolTip(
        QString("%1 movement, timing and other control instructions are preserved automatically.")
            .arg(controls));
    preview_->setFrameEditing(true);
    preview_->setFrameBounds(asset ? QRectF(-asset->anchor, asset->size) : QRectF{});
    updateFrameTools();
}
void AnimationSequenceEditor::selectVisibleFrames() {
    if (refreshing_ || !frames_ || !frames_->currentItem())
        return;
    guarded([this] {
        // Ctrl-click can deselect the current item while other cards remain
        // selected. Painting must follow an actually selected frame.
        if (!frames_->currentItem()->isSelected() && !frames_->selectedItems().isEmpty()) {
            const QSignalBlocker block(frames_);
            frames_->setCurrentItem(frames_->selectedItems().first(), QItemSelectionModel::NoUpdate);
        }
        const QSignalBlocker block(records_);
        records_->clearSelection();
        records_->setCurrentCell(frames_->currentItem()->data(Qt::UserRole).toInt(), 0,
                                 QItemSelectionModel::NoUpdate);
        for (int row : selectedFrameRecords())
            records_->selectionModel()->select(records_->model()->index(row, 0),
                                               QItemSelectionModel::Select |
                                                   QItemSelectionModel::Rows);
        selectRecord();
    });
}
void AnimationSequenceEditor::updateFrameTools() {
    if (!frames_ || !guided_)
        return;
    durationTyped_ = false;
    const auto selected = selectedFrameRecords();
    const int index = frames_->currentRow();
    duration_->setEnabled(!selected.isEmpty());
    findChild<QPushButton *>("sequenceDuplicate")->setEnabled(!selected.isEmpty());
    findChild<QPushButton *>("sequenceDelete")->setEnabled(!selected.isEmpty());
    paint_->setEnabled(selected.size() == 1);
    layersButton_->setEnabled(selected.size() == 1);
    findChild<QPushButton *>("sequenceCopyArtwork")->setEnabled(selected.size() == 1);
    earlier_->setEnabled(selected.size() == 1 && index > 0);
    later_->setEnabled(selected.size() == 1 && index + 1 < frames_->count());
    frameSelection_->setText(selected.size() == 1
                                 ? QString("Frame %1 selected").arg(index + 1)
                                 : QString("%1 frames selected").arg(selected.size()));
    if (!selected.isEmpty()) {
        const auto records = descriptor().poses().effective(poses_->currentData().toInt()).records;
        const int first = *std::min_element(selected.begin(), selected.end());
        const int ticks = u8(records[first].operands, records[first].operands.size() - 1);
        const QSignalBlocker block(duration_);
        duration_->setValue(ticks);
        bool mixed = false;
        for (int row : selected)
            mixed |= u8(records[row].operands, records[row].operands.size() - 1) != ticks;
        duration_->setPrefix(mixed ? "Mixed · " : "");
    }
    preview_->setPreviousFrame(
        onion_->isChecked() && index > 0
            ? std::optional<int>(frames_->item(index - 1)->data(Qt::UserRole + 2).toInt())
            : std::nullopt);
}
void AnimationSequenceEditor::changeFrameDuration(int ticks) {
    const auto selected = selectedFrameRecords();
    if (selected.isEmpty())
        return;
    // Avoid detaching untouched artwork when a spinbox commits an unchanged value.
    const auto records = descriptor().poses().effective(poses_->currentData().toInt()).records;
    bool changed = false;
    for (int row : selected)
        changed |= u8(records[row].operands, records[row].operands.size() - 1) != ticks;
    if (!changed)
        return;
    edit("Change frame duration", [&](PoseBank &bank, int state, int) {
        for (int row : selected)
            bank.setFrame(state, row, frameToken(records[row]), ticks);
    });
}
void AnimationSequenceEditor::duplicateVisibleFrames() {
    auto selected = selectedFrameRecords().values();
    std::sort(selected.begin(), selected.end());
    if (selected.isEmpty())
        return;
    pendingRecord_ = selected.first() + 1;
    try {
        edit("Duplicate animation frames", [&](PoseBank &bank, int state, int) {
            const auto before = bank.effective(state).records;
            int added = 0;
            for (int row : selected) {
                bank.insertFrame(state, row + 1 + added++, frameToken(before[row]),
                                 u8(before[row].operands, before[row].operands.size() - 1));
            }
        });
    } catch (...) {
        pendingRecord_ = -1;
        throw;
    }
}
void AnimationSequenceEditor::removeVisibleFrames() {
    auto selected = selectedFrameRecords().values();
    std::sort(selected.begin(), selected.end(), std::greater<int>());
    if (selected.isEmpty())
        return;
    if (selected.size() == frames_->count())
        throw Error("Keep at least one frame in the animation.");
    pendingRecord_ = std::max(0, selected.last() - 1);
    try {
        edit("Delete animation frames", [&](PoseBank &bank, int state, int) {
            for (int row : selected)
                bank.removeFrame(state, row);
        });
    } catch (...) {
        pendingRecord_ = -1;
        throw;
    }
}
void AnimationSequenceEditor::moveVisibleFrame(int from, int to) {
    if (!frames_ || from < 0 || to < 0 || from >= frames_->count() || to >= frames_->count() ||
        from == to)
        return;
    const int source = frames_->item(from)->data(Qt::UserRole).toInt(),
              target = frames_->item(to)->data(Qt::UserRole).toInt();
    pendingRecord_ = target;
    try {
        edit("Reorder animation frame",
             [=](PoseBank &bank, int state, int) { bank.moveFrame(state, source, target); });
    } catch (...) {
        pendingRecord_ = -1;
        throw;
    }
}
void AnimationSequenceEditor::moveFrameArtwork(QPoint offset) {
    const auto selected = selectedFrameRecords();
    if (selected.isEmpty())
        return;
    translateFrames({{poses_->currentData().toInt(), selected}}, offset, -1, true);
}
} // namespace lordly
