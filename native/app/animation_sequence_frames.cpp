#include "app/actor_pose_editor.h"
#include "app/animation_sequence_editor.h"
#include "app/combat_animation_replace.h"
#include "engine/sprite_sources.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDrag>
#include <QDropEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTimer>
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
    std::function<void(int, int)> durationChanged;
    bool eventFilter(QObject *object, QEvent *event) override {
        auto spin = qobject_cast<QSpinBox *>(object);
        if (spin && event->type() == QEvent::KeyPress) {
            const auto key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                spin->interpretText();
                spin->setProperty("pendingTicks", false);
                for (auto timer : spin->findChildren<QTimer *>()) timer->stop();
                emit commitData(spin);
                return true; // Persistent controls stay open; no queued close/commit.
            }
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        auto card = option;
        card.rect.adjust(0, 0, 0, -30);
        QStyledItemDelegate::paint(p, card, index);
        if (index.data(Qt::UserRole + 1).toBool()) {
            p->save();
            p->setPen(QPen(QColor("#62bd91"), 3));
            p->drawRoundedRect(option.rect.adjusted(2, 2, -2, -2), 4, 4);
            p->restore();
        }
    }
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override {
        auto spin = new QSpinBox(parent);
        spin->setObjectName("sequenceFrameTicks");
        spin->setRange(0, 255);
        spin->setSuffix(" ticks");
        spin->setAlignment(Qt::AlignCenter);
        spin->setKeyboardTracking(false);
        spin->setToolTip("This frame's duration. Type a value and press Enter, or use the arrows.");
        auto timer = new QTimer(spin);
        timer->setSingleShot(true);
        timer->setInterval(250);
        const auto commit = [this, spin, timer] {
            if (!spin->property("pendingTicks").toBool()) return;
            timer->stop();
            spin->setProperty("pendingTicks", false);
            emit const_cast<FrameDelegate *>(this)->commitData(spin);
        };
        connect(spin, &QSpinBox::valueChanged, spin, [spin, timer] {
            spin->setProperty("pendingTicks", true);
            timer->start();
        });
        connect(timer, &QTimer::timeout, spin, commit);
        connect(spin, &QSpinBox::editingFinished, spin, commit);
        return spin;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        auto spin = qobject_cast<QSpinBox *>(editor);
        if (spin->property("pendingTicks").toBool()) return;
        const QSignalBlocker block(spin);
        spin->setProperty("frameRecord", index.data(Qt::UserRole));
        spin->setValue(index.data(Qt::UserRole + 3).toInt());
        if (!spin->hasFocus()) spin->findChild<QLineEdit *>()->deselect();
    }
    void destroyEditor(QWidget *editor, const QModelIndex &index) const override {
        editor->setProperty("pendingTicks", false);
        editor->removeEventFilter(const_cast<FrameDelegate *>(this));
        for (auto timer : editor->findChildren<QTimer *>()) timer->stop();
        QStyledItemDelegate::destroyEditor(editor, index);
    }
    void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const override {
        if (durationChanged)
            durationChanged(index.data(Qt::UserRole).toInt(), qobject_cast<QSpinBox *>(editor)->value());
    }
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override {
        editor->setGeometry(option.rect.adjusted(4, option.rect.height() - 29, -4, -2));
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
    auto exports = new QHBoxLayout;
    auto exportButton = exportButton_ = new QPushButton("Export…");
    exportButton->setObjectName("sequenceExport");
    auto exportMenu = new QMenu(exportButton);
    auto png = exportMenu->addAction("Selected frame — PNG…");
    png->setObjectName("sequenceExportPng");
    png->setToolTip("Save the selected frame, including equipment and project edits, on a transparent background.");
    const auto addAnimation = [&](const QString &label, const char *name, AnimationExport format) {
        auto action = exportMenu->addAction(label);
        action->setObjectName(name);
        action->setToolTip(format == AnimationExport::Webm
            ? "Preview playback with transparency. Requires FFmpeg; video colors can differ slightly."
            : "Preview playback with exact rendered colors and transparency. No extra software needed.");
        connect(action, &QAction::triggered, this, [this, format] {
            guarded([this, format] { chooseAnimationExport(format); });
        });
    };
    addAnimation("Animation — Animated PNG…", "sequenceExportApng", AnimationExport::AnimatedPng);
    addAnimation("Frame sequence — ZIP…", "sequenceExportZip", AnimationExport::FramesZip);
    addAnimation("Video — WebM…", "sequenceExportWebm", AnimationExport::Webm);
    exportMenu->addSeparator();
    for (bool selected : {true, false}) {
        auto action = exportMenu->addAction(selected ? "Selected frame → Art Library"
                                                     : "Animation playback → Art Library");
        action->setToolTip(selected ? "Save this frame as reusable artwork."
                                    : "Save one cycle exactly as previewed, including movement and "
                                      "timing at the selected speed.");
        action->setObjectName(selected ? "sequenceLibrarySelected" : "sequenceLibraryAll");
        connect(action, &QAction::triggered, this,
                [this, selected] { guarded([this, selected] { copyToLibrary(selected); }); });
    }
    exportButton->setMenu(exportMenu);
    exports->addWidget(exportButton);
    exports->addStretch();
    layout->addLayout(exports);
    connect(png, &QAction::triggered, this, [this] { guarded([this] { chooseFrameExport(); }); });
    auto split = new QSplitter;
    split->setObjectName("sequenceFrameSplit");
    split->setChildrenCollapsible(false);
    layout->addWidget(split, 1);
    auto left = new QWidget;
    left->setMinimumWidth(430);
    auto column = new QVBoxLayout(left);
    column->setContentsMargins(0, 0, 0, 0);
    auto heading = new QHBoxLayout;
    frameHeading_ = new QLabel("Frames");
    frameHeading_->setObjectName("sequenceFrameHeading");
    heading->addWidget(frameHeading_, 1);
    auto tools = new QPushButton("Advanced");
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
    layersButton_ = menu->addAction("Edit selected frame layers…");
    layersButton_->setObjectName("sequenceLayers");
    connect(layersButton_, &QAction::triggered, this,
            [this] { guarded([this] { editLayers(); }); });
    legacyAction("Compare with another animation…", "sequenceCompare");
    menu->addSeparator();
    auto restore = menu->addAction("Restore all animations for this side and artwork…");
    connect(restore, &QAction::triggered, this, [this] {
        if (QMessageBox::question(
                this, "Restore animation set",
                "Restore every animation for this class, battle side and artwork variant?\n\n"
                "This includes animations other than the one currently selected. You can Undo this "
                "change.",
                QMessageBox::RestoreDefaults | QMessageBox::Cancel,
                QMessageBox::Cancel) == QMessageBox::RestoreDefaults)
            remove_->click();
    });
    connect(menu, &QMenu::aboutToShow, this,
            [this, restore] { restore->setEnabled(remove_->isEnabled()); });
    tools->setMenu(menu);
    exports->insertWidget(1, tools);
    auto add = new QPushButton("Add frames…");
    add->setObjectName("sequenceAdd");
    add->setToolTip(
        "Insert frames after the selected frame, from another animation, Art Library or files.");
    heading->addWidget(add);
    connect(add, &QPushButton::clicked, this,
            [this] { guarded([this] { chooseFrameSource(false); }); });
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
    auto hint = new QLabel(
        "Select a frame to edit its sprite. Drag to reorder; type ticks below each frame.");
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
    frames_->setGridSize({124, 160});
    frames_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    frames_->setUniformItemSizes(true);
    frames_->setWordWrap(false);
    frames_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    frames_->setDragDropMode(QAbstractItemView::InternalMove);
    frames_->setDefaultDropAction(Qt::MoveAction);
    frames_->setDropIndicatorShown(true);
    auto delegate = new FrameDelegate(frames_);
    delegate->durationChanged = [this](int record, int ticks) {
        if (!refreshing_) guarded([&] { changeFrameDuration(record, ticks); });
    };
    frames_->setItemDelegate(delegate);
    frames_->setMinimumHeight(155);
    board->move = [this](int from, int to) { guarded([&] { moveVisibleFrame(from, to); }); };
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
    const auto button = [&](const QString &label, const char *name, int row, int col,
                            const std::function<void()> &action) {
        auto b = new QPushButton(label);
        b->setObjectName(name);
        toolbar->addWidget(b, row, col);
        connect(b, &QPushButton::clicked, this, [this, action] { guarded(action); });
        return b;
    };
    paint_ = button("Edit sprite…", "sequencePaint", 1, 0, [this] { editSprite(); });
    auto replace =
        button("Replace frame…", "sequenceCopyArtwork", 1, 1, [this] { chooseFrameSource(true); });
    replace->setToolTip("Replace one frame's artwork from another animation, Art Library or files. "
                        "Keeps its duration.");
    button("Duplicate", "sequenceDuplicate", 1, 2, [this] { duplicateVisibleFrames(); });
    button("Delete", "sequenceDelete", 1, 3, [this] { removeVisibleFrames(); });
    column->addWidget(frameTools_);
    column->addWidget(frames_, 1);
    auto frameMenu = new QMenu(frames_);
    frameMenu->setObjectName("sequenceFrameMenu");
    for (const auto &entry :
         {qMakePair("Edit sprite…", "sequencePaint"),
          qMakePair("Replace frame…", "sequenceCopyArtwork"),
          qMakePair("Duplicate", "sequenceDuplicate"), qMakePair("Delete", "sequenceDelete")}) {
        auto action = frameMenu->addAction(entry.first);
        auto control = frameTools_->findChild<QPushButton *>(entry.second);
        connect(action, &QAction::triggered, control, &QPushButton::click);
        connect(frameMenu, &QMenu::aboutToShow, action,
                [action, control] { action->setEnabled(control->isEnabled()); });
    }
    frameMenu->addSeparator();
    earlier_ = frameMenu->addAction("Move earlier");
    later_ = frameMenu->addAction("Move later");
    connect(earlier_, &QAction::triggered, this, [this] {
        guarded([this] { moveVisibleFrame(frames_->currentRow(), frames_->currentRow() - 1); });
    });
    connect(later_, &QAction::triggered, this, [this] {
        guarded([this] { moveVisibleFrame(frames_->currentRow(), frames_->currentRow() + 1); });
    });
    frames_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(frames_, &QListWidget::customContextMenuRequested, this, [this, frameMenu](QPoint at) {
        if (auto item = frames_->itemAt(at)) {
            if (!item->isSelected())
                frames_->setCurrentItem(item, QItemSelectionModel::ClearAndSelect);
            frameMenu->exec(frames_->viewport()->mapToGlobal(at));
        }
    });
    for (const auto &entry : {qMakePair(QKeySequence(Qt::Key_Delete), "sequenceDelete"),
                              qMakePair(QKeySequence(Qt::CTRL | Qt::Key_D), "sequenceDuplicate")}) {
        auto shortcut = new QShortcut(entry.first, frames_);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, this,
                [this, name = entry.second] { findChild<QPushButton *>(name)->click(); });
    }
    connect(preview_, &ActorPoseEditor::frameArtworkMoved, this,
            [this](QPoint offset, int layer) { guarded([&] { moveFrameArtwork(offset, layer); }); });
    frameStatus_ = new QLabel;
    frameStatus_->setWordWrap(true);
    frameStatus_->setObjectName("sequenceFrameStatus");
    column->addWidget(frameStatus_);
    split->addWidget(left);
    auto right = framePreviewHost_ = new QWidget;
    right->setMinimumWidth(340);
    auto previewLayout = new QVBoxLayout(right);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addWidget(new QLabel("<b>Preview</b>"));
    emptyPreview_ = new QLabel(
        "<b>No frames yet</b><br><br>Add frames from another animation, Art Library or files.<br>"
        "Use Create animation to import a complete sequence.");
    emptyPreview_->setObjectName("sequenceEmptyPreview");
    emptyPreview_->setWordWrap(true);
    emptyPreview_->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(emptyPreview_, 1);
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
    auto note = new QLabel(
        "Drag the selected sprite to position it. Scroll to zoom; drag the background to pan.");
    note->setWordWrap(true);
    previewLayout->addWidget(note);
    split->insertWidget(0, right);
    split->setSizes({480, 620});
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
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
    const bool timingOnly = durationEdit_ && frameContext_ == context;
    frameContext_ = context;
    pendingRecord_ = -1;
    const QSignalBlocker blocker(frames_);
    const auto snapshot = descriptor();
    const auto bank = snapshot.poses();
    const auto records = bank.effective(poses_->currentData().toInt()).records;
    int total = 0, controls = 0;
    if (timingOnly) {
        // Keep the spinbox receiving input alive. Timing edits do not change
        // artwork, so there is no reason to decode every thumbnail again.
        for (int i = 0; i < frames_->count(); ++i) {
            auto item = frames_->item(i);
            const auto &record = records[item->data(Qt::UserRole).toInt()];
            const int ticks = u8(record.operands, record.operands.size() - 1);
            item->setData(Qt::UserRole + 3, ticks);
            item->setToolTip(QString("Frame %1 · %2 ticks\nDouble-click artwork to paint.").arg(i + 1).arg(ticks));
            total += ticks;
        }
        frameHeading_->setText(QString("%1 frames · %2 ticks").arg(frames_->count()).arg(total));
        updateFrameTools();
        return;
    }
    frames_->clear();
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
            icon, QString("Frame %1").arg(index + 1), frames_);
        item->setSizeHint({118, 155});
        item->setTextAlignment(Qt::AlignHCenter);
        item->setData(Qt::UserRole, r);
        item->setData(Qt::UserRole + 2, frameToken(records[r]));
        item->setData(Qt::UserRole + 3, ticks);
        item->setToolTip(QString("Frame %1 · %2 ticks\nDouble-click to paint.%3")
                             .arg(index + 1)
                             .arg(ticks)
                             .arg(thumbnailError.isEmpty()
                                      ? QString{}
                                      : "\nPreview unavailable: " + thumbnailError));
        if (r == desired)
            current = index;
        item->setSelected(selection.contains(r));
        frames_->openPersistentEditor(item);
        if (auto spin = qobject_cast<QSpinBox *>(frames_->itemWidget(item)))
            spin->findChild<QLineEdit *>()->deselect();
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
    frameStatus_->setText(
        thumbnailError.isEmpty()
            ? "Edits stay with this class. Undo is available; save your project to keep changes."
            : "Some frame previews could not be drawn: " + thumbnailError);
    frameStatus_->setToolTip(
        QString("%1 movement, timing and other control instructions are preserved automatically.")
            .arg(controls));
    preview_->setVisible(frames_->count() > 0);
    emptyPreview_->setVisible(frames_->count() == 0);
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
    const auto selected = selectedFrameRecords();
    const int index = frames_->currentRow();
    exportButton_->findChild<QAction *>("sequenceExportPng")->setEnabled(selected.size() == 1);
    exportButton_->findChild<QAction *>("sequenceLibrarySelected")
        ->setEnabled(selected.size() == 1);
    exportButton_->setEnabled(frames_->count() > 0);
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
    preview_->setPreviousFrame(
        onion_->isChecked() && index > 0
            ? std::optional<int>(frames_->item(index - 1)->data(Qt::UserRole + 2).toInt())
            : std::nullopt);
}
void AnimationSequenceEditor::chooseFrameSource(bool replace) {
    const auto before = project_->animationSequences()->json();
    const auto overrides = project_->animationOverrides()->json();
    const int row = frames_->currentItem() && !selectedFrameRecords().isEmpty()
                        ? frames_->currentItem()->data(Qt::UserRole).toInt()
                        : -1;
    if (replace && selectedFrameRecords().size() != 1)
        throw Error("Select one frame to replace");
    const auto next =
        chooseCombatAnimationFrames(*project_, selectedClass(), routes_->currentIndex(),
                                    poses_->currentData().toInt(), row, replace, this);
    if (!next)
        return;
    if (project_->animationSequences()->json() != before ||
        project_->animationOverrides()->json() != overrides)
        throw Error("Animations changed while the import dialog was open. Reopen it to use the "
                    "latest version.");
    if (!replace)
        pendingRecord_ = row >= 0 ? row + 1 : 0;
    publish(*next, replace ? "Replace animation frame" : "Add animation frames");
}
void AnimationSequenceEditor::changeFrameDuration(int row, int ticks) {
    // Avoid detaching untouched artwork when a spinbox commits an unchanged value.
    const auto records = descriptor().poses().effective(poses_->currentData().toInt()).records;
    if (row < 0 || row >= records.size() || !visibleFrame(records[row]) ||
        u8(records[row].operands, records[row].operands.size() - 1) == ticks)
        return;
    const QScopedValueRollback timing(durationEdit_, true);
    edit("Change frame duration", [&](PoseBank &bank, int state, int) {
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
void AnimationSequenceEditor::moveFrameArtwork(QPoint offset, int layer) {
    const auto selected = selectedFrameRecords();
    if (selected.isEmpty())
        return;
    translateFrames({{poses_->currentData().toInt(), selected}}, offset, layer, true);
}
} // namespace lordly
