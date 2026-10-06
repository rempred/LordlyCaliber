#include "app/scene_preview.h"
#include "engine/class_data.h"
#include "engine/save_document.h"
#include "engine/scene_images.h"
#include "engine/scene_actions.h"
#include "engine/scene_document_compiler.h"
#include "engine/scene_story_cast.h"
#include "engine/text.h"
#include <QComboBox>
#include <QApplication>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFile>
#include <QJsonArray>
#include <QMessageBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <bit>
namespace lordly {
ScenePreview::ScenePreview(const Rom &rom, QWidget *parent)
    : QWidget(parent), rom_(rom), renderer_(rom), spriteRenderer_(rom), stageRenderer_(rom),
      objectRenderer_(rom) {
    auto layout = new QVBoxLayout(this);
    context_ = new QLabel("Select a scene to preview its current commands.");
    context_->setObjectName("scenePreviewContext");
    context_->setWordWrap(true);
    layout->addWidget(context_);
    auto inputRow = new QHBoxLayout;
    auto importInputs = new QPushButton("Import playback inputs…");
    importInputs->setObjectName("sceneImportPlaybackInputs");
    clearInputs_ = new QPushButton("Clear playback inputs");
    clearInputs_->setObjectName("sceneClearPlaybackInputs");
    inputRow->addWidget(importInputs); inputRow->addWidget(clearInputs_); inputRow->addStretch();
    layout->addLayout(inputRow);
    inputsStatus_ = new QLabel;
    inputsStatus_->setObjectName("scenePlaybackInputsStatus"); inputsStatus_->setWordWrap(true);
    layout->addWidget(inputsStatus_);
    connect(importInputs, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Import playback inputs", {}, "Playback inputs (*.json)");
        if (path.isEmpty()) return;
        try {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) throw Error(file.errorString());
            importPlaybackInputs(file.read(128 * 1024 + 1));
        } catch (const std::exception &e) { QMessageBox::warning(this, "Playback inputs", QString::fromUtf8(e.what())); }
    });
    connect(clearInputs_, &QPushButton::clicked, this, &ScenePreview::clearPlaybackInputs);
    launchImageUi_ = new QWidget;
    auto launchRow = new QHBoxLayout(launchImageUi_);
    launchRow->setContentsMargins(0, 0, 0, 0);
    launchRow->addWidget(new QLabel("Launch image"));
    launchImage_ = new QComboBox;
    launchImage_->setObjectName("sceneLaunchImage");
    launchImage_->setToolTip(
        "Preview the image supplied by the launch event. This choice does not edit the scene.");
    launchRow->addWidget(launchImage_, 1);
    launchImageUi_->hide();
    layout->addWidget(launchImageUi_);
    launchContextUi_ = new QWidget;
    auto contextRow = new QHBoxLayout(launchContextUi_);
    contextRow->setContentsMargins(0, 0, 0, 0);
    contextRow->addWidget(new QLabel("Launch context"));
    launchContext_ = new QComboBox;
    launchContext_->setObjectName("sceneLaunchContext");
    launchContext_->setToolTip("Choose the event whose preceding scene supplies Actors, dialogue "
                               "and scenery. This does not change the ROM.");
    contextRow->addWidget(launchContext_, 1);
    layout->addWidget(launchContextUi_);
    launchContextUi_->hide();
    storyCastUi_ = new QWidget;
    auto castRow = new QGridLayout(storyCastUi_);
    castRow->setContentsMargins(0, 0, 0, 0);
    castRow->addWidget(new QLabel("Primary class"), 0, 0);
    castRow->addWidget(new QLabel("Secondary class"), 1, 0);
    const auto classes = ClassData::load(rom_);
    classCount_ = int(classes.entries().size());
    const auto sample = SceneStoryCast::sample();
    castSource_ = new QLabel("Sample cast");
    castSource_->setObjectName("sceneCastSource");
    castSource_->setWordWrap(true);
    auto changedCast = [this] {
        if (!storyCastEnabled_)
            return;
        armyCastActive_ = false;
        castSource_->setText("Manual preview cast");
        inputHistory_.clear();
        pressedHistory_.clear();
        restart();
    };
    for (int i = 0; i < 5; ++i) {
        auto choice = storyCast_[i] = new QComboBox;
        auto secondary = storySecondary_[i] = new QComboBox;
        choice->setObjectName(QString("sceneCast%1").arg(i));
        secondary->setObjectName(QString("sceneCastSecondary%1").arg(i));
        choice->setAccessibleName(QString("Actor %1 primary class").arg(i));
        secondary->setAccessibleName(QString("Actor %1 secondary class").arg(i));
        choice->setToolTip("Primary class controls this story Actor's dialogue branch.");
        secondary->setToolTip(
            "Retain the independent secondary class supplied by the launch event.");
        for (int id : SceneStoryCast::choices())
            if (id < classCount_)
                choice->addItem(decodeText(classes.entries()[id].name), id);
        for (int id = 1; id < classCount_; ++id)
            secondary->addItem(decodeText(classes.entries()[id].name), id);
        choice->setCurrentIndex(choice->findData(sample[i].primary));
        secondary->setCurrentIndex(secondary->findData(sample[i].secondary));
        castRow->addWidget(choice, 0, i + 1);
        castRow->addWidget(secondary, 1, i + 1);
        castRow->setColumnStretch(i + 1, 1);
        connect(choice, &QComboBox::currentIndexChanged, this, [this, i, changedCast] {
            const QSignalBlocker guard(storySecondary_[i]);
            storySecondary_[i]->setCurrentIndex(
                storySecondary_[i]->findData(storyCast_[i]->currentData()));
            changedCast();
        });
        connect(secondary, &QComboBox::currentIndexChanged, this, changedCast);
    }
    auto castTools = new QHBoxLayout;
    armyCast_ = new QPushButton("Use current army");
    armyCast_->setObjectName("sceneUseArmyCast");
    armyCast_->setEnabled(false);
    armyCast_->setToolTip("Select this story's cast from the current Save workspace roster, including native eligibility and fallbacks.");
    savedCast_ = new QPushButton("Use saved launch cast");
    savedCast_->setObjectName("sceneUseSavedCast");
    savedCast_->setEnabled(false);
    savedCast_->setToolTip("Use the five class pairs stored in the Save workspace's selected slot. "
                           "This does not rerun the story event's army selection.");
    auto resetCast = new QPushButton("Use sample cast");
    resetCast->setObjectName("sceneResetCast");
    castTools->addWidget(armyCast_);
    castTools->addWidget(savedCast_);
    castTools->addWidget(resetCast);
    castTools->addWidget(castSource_, 1);
    castRow->addLayout(castTools, 2, 0, 1, 6);
    auto applyCast = [this](const std::array<StoryCastMember, 5> &cast, const QString &source) {
        armyCastActive_ = false;
        for (int i = 0; i < 5; ++i) {
            const QSignalBlocker primary(storyCast_[i]), secondary(storySecondary_[i]);
            storyCast_[i]->setCurrentIndex(storyCast_[i]->findData(cast[i].primary));
            storySecondary_[i]->setCurrentIndex(storySecondary_[i]->findData(cast[i].secondary));
        }
        castSource_->setText(source);
        inputHistory_.clear();
        pressedHistory_.clear();
        if (storyCastEnabled_)
            restart();
    };
    connect(resetCast, &QPushButton::clicked, this,
            [applyCast] { applyCast(SceneStoryCast::sample(), "Sample cast"); });
    connect(armyCast_, &QPushButton::clicked, this, [this] {
        try {
            loadArmyCast();
            armyCastActive_ = true;
            inputHistory_.clear();
            pressedHistory_.clear();
            if (storyCastEnabled_) restart();
        } catch (const Error &e) {
            castSource_->setText(QString("Cast unchanged: %1").arg(QString::fromUtf8(e.what())));
        }
    });
    connect(savedCast_, &QPushButton::clicked, this, [this, applyCast] {
        try {
            const auto save = saveProvider_ ? saveProvider_() : nullptr;
            if (!save)
                throw Error("Open a save in the Save workspace and select a slot first");
            const auto cast = SceneStoryCast::saved(*save, classCount_);
            const auto slot = save->selectedSlot();
            applyCast(cast, slot < 0     ? "Saved launch cast from the open runtime state"
                            : slot == 15 ? "Saved launch cast from the suspend slot"
                                         : QString("Saved launch cast from slot %1").arg(slot + 1));
        } catch (const Error &e) {
            castSource_->setText(QString("Cast unchanged: %1").arg(QString::fromUtf8(e.what())));
        }
    });
    storyCastUi_->hide();
    layout->addWidget(storyCastUi_);
    dialogueNamesUi_ = new QWidget;
    auto names = new QHBoxLayout(dialogueNamesUi_);
    names->setContentsMargins(0, 0, 0, 0);
    names->addWidget(new QLabel("Preview names"));
    const QString labels[]{"Protagonist", "Speaker", "Army", "Unit leader"};
    const QString defaults[]{"Magnus", "Magnus", "Ogre", "Magnus"};
    for (int i = 0; i < 4; ++i) {
        names->addWidget(new QLabel(labels[i]));
        auto field = dialogueNames_[i] = new QLineEdit(defaults[i]);
        field->setObjectName(QString("sceneDialogueName%1").arg(i));
        field->setAccessibleName("Preview " + labels[i]);
        field->setMaxLength(63);
        names->addWidget(field, 1);
        connect(field, &QLineEdit::editingFinished, this, [this] {
            inputHistory_.clear();
            pressedHistory_.clear();
            restart();
        });
    }
    dialogueNamesUi_->hide();
    layout->addWidget(dialogueNamesUi_);
    rewardUi_ = new QWidget;
    auto rewardRow = new QHBoxLayout(rewardUi_);
    rewardRow->setContentsMargins(0, 0, 0, 0);
    rewardRow->addWidget(new QLabel("Preview reward"));
    reward_ = new QComboBox;
    reward_->setObjectName("sceneReward");
    reward_->setToolTip(
        "Declared sample reward; the game supplies this item from the parent event.");
    for (int i = 1; i < 278; ++i)
        reward_->addItem(ScenePanels::rewardName(rom_, quint16(i)), i);
    for (int i = 1; i < 45; ++i)
        reward_->addItem(ScenePanels::rewardName(rom_, quint16(0x200 | i)), 0x200 | i);
    rewardRow->addWidget(reward_, 1);
    rewardUi_->hide();
    layout->addWidget(rewardUi_);
    connect(reward_, &QComboBox::currentIndexChanged, this, [this] {
        inputHistory_.clear();
        pressedHistory_.clear();
        restart();
    });
    auto controls = new QHBoxLayout;
    play_ = new QPushButton("Play");
    play_->setCheckable(true);
    play_->setObjectName("scenePlay");
    step_ = new QPushButton("Step");
    step_->setObjectName("sceneStep");
    auto reset = new QPushButton("Restart"), fit = new QPushButton("Fit");
    reset->setObjectName("sceneRestart");
    position_ = new QSpinBox;
    position_->setObjectName("scenePosition");
    position_->setRange(0, 10000);
    position_->setKeyboardTracking(false);
    rate_ = new QSpinBox;
    rate_->setRange(1, 120);
    rate_->setValue(30);
    rate_->setSuffix(" steps/s");
    controls->addWidget(play_);
    controls->addWidget(step_);
    skip_ = new QPushButton("Skip scene (Enter)");
    skip_->setObjectName("sceneSkip");
    controls->addWidget(skip_);
    connect(skip_, &QPushButton::clicked, this, &ScenePreview::skipScene);
    controls->addWidget(reset);
    controls->addWidget(new QLabel("Step"));
    controls->addWidget(position_);
    controls->addWidget(rate_);
    controls->addStretch();
    controls->addWidget(fit);
    layout->addLayout(controls);
    imageControlsUi_ = new QWidget;
    imageControlsUi_->setObjectName("sceneImageControls");
    auto imageButtons = new QHBoxLayout(imageControlsUi_);
    imageButtons->setContentsMargins(0, 0, 0, 0);
    imageButtons->addWidget(new QLabel("Image controls"));
    const struct {
        const char *label;
        const char *name;
        quint16 bit;
    } buttons[]{{"Left", "imagePanLeft", 0x200}, {"Right", "imagePanRight", 0x100},
                {"Up", "imagePanUp", 0x800},     {"Down", "imagePanDown", 0x400},
                {"Zoom in", "imageZoomIn", 8},   {"Zoom out", "imageZoomOut", 4}};
    for (const auto &spec : buttons) {
        auto button = new QPushButton(QString::fromLatin1(spec.label));
        button->setObjectName(QString::fromLatin1(spec.name));
        imageButtons->addWidget(button);
        connect(button, &QPushButton::pressed, this,
                [this, bit = spec.bit] { imageButton(bit, true); });
        connect(button, &QPushButton::released, this,
                [this, bit = spec.bit] { imageButton(bit, false); });
    }
    imageButtons->addWidget(new QLabel("Arrow keys · + / − to zoom"));
    imageButtons->addStretch();
    imageControlsUi_->hide();
    layout->addWidget(imageControlsUi_);
    panelControlsUi_ = new QWidget;
    auto panelButtons = new QHBoxLayout(panelControlsUi_);
    panelButtons->setContentsMargins(0, 0, 0, 0);
    panelButtons->addWidget(new QLabel("Dialogue / panel controls"));
    const struct {
        const char *label;
        const char *name;
        quint16 bit;
    } panelSpecs[]{{"Up", "panelUp", 0x800},
                   {"Down", "panelDown", 0x400},
                   {"Confirm (A)", "panelConfirm", 0x8000},
                   {"Cancel (B)", "panelCancel", 0x4000}};
    for (const auto &spec : panelSpecs) {
        auto button = new QPushButton(QString::fromLatin1(spec.label));
        button->setObjectName(QString::fromLatin1(spec.name));
        panelButtons->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, bit = spec.bit] { panelButton(bit); });
    }
    panelButtons->addWidget(new QLabel("Arrow keys · A / B"));
    panelButtons->addStretch();
    panelControlsUi_->hide();
    layout->addWidget(panelControlsUi_);
    canvas_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(canvas_);
    view_->setObjectName("sceneCanvas");
    view_->installEventFilter(this);
    view_->viewport()->installEventFilter(this);
    view_->setToolTip("Click a visible Actor to select it; double-click to find its action. Drag to pan.");
    view_->setBackgroundBrush(QColor("#252931"));
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    layout->addWidget(view_, 1);
    status_ = new QLabel;
    status_->setObjectName("scenePlaybackStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    seekTimer_ = new QTimer(this);
    seekTimer_->setInterval(0);
    connect(seekTimer_, &QTimer::timeout, this, &ScenePreview::continueSeek);
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &ScenePreview::step);
    connect(play_, &QPushButton::toggled, this, [this](bool checked) {
        if (preparing_ && checked) {
            seekTimer_->start();
            play_->setText("Pause setup");
            return;
        }
        if (checked && current_ && !current_->stopped) {
            timer_->start(qMax(1, qRound(1000. / rate_->value())));
            play_->setText("Pause");
        } else
            stop();
    });
    connect(step_, &QPushButton::clicked, this, [this] {
        stop();
        step();
    });
    connect(reset, &QPushButton::clicked, this, &ScenePreview::restart);
    auto actors = new QHBoxLayout;
    actors->addWidget(new QLabel("Live object"));
    liveObjects_ = new QComboBox;
    liveObjects_->setObjectName("sceneLiveActors");
    liveObjects_->setMinimumContentsLength(10);
    actors->addWidget(liveObjects_);
    objectState_ = new QLabel("No active Actors");
    objectState_->setObjectName("sceneActorState");
    objectState_->setWordWrap(true);
    actors->addWidget(objectState_, 1);
    findObject_ = new QPushButton("Find Actor action");
    findObject_->setObjectName("sceneFindActorAction");
    findObject_->setToolTip("Find the selected slot's placement or first supported action in the "
                            "current script. Conditions may give a slot more than one action.");
    findObject_->setEnabled(false);
    findObject_->hide();
    actors->addWidget(findObject_);
    layout->addLayout(actors);
    auto placement = new QHBoxLayout;
    placementHelp_ = new QLabel;
    placementHelp_->setObjectName("scenePlacementHelp");
    placementHelp_->setWordWrap(true);
    placement->addWidget(placementHelp_, 1);
    cancelPlacement_ = new QPushButton("Cancel positioning");
    cancelPlacement_->setObjectName("sceneCancelPosition");
    cancelPlacement_->hide();
    placement->addWidget(cancelPlacement_);
    layout->addLayout(placement);
    connect(cancelPlacement_, &QPushButton::clicked, this, &ScenePreview::cancelPositionPick);
    connect(liveObjects_, &QComboBox::currentIndexChanged, this, [this] {
        if (liveObjects_->currentIndex() >= 0)
            selectedObject_ = {SceneObjectKind(liveObjects_->currentData(Qt::UserRole + 1).toInt()),
                               liveObjects_->currentData().toInt()};
        presentSelectedObject();
    });
    connect(findObject_, &QPushButton::clicked, this, [this] {
        if (!current_ || liveObjects_->currentIndex() < 0)
            return;
        stop();
        findSelectedAction();
    });
    connect(position_, &QSpinBox::valueChanged, this, &ScenePreview::seek);
    connect(launchImage_, &QComboBox::currentIndexChanged, this, [this] {
        inputHistory_.clear();
        pressedHistory_.clear();
        restart();
    });
    connect(launchContext_, &QComboBox::currentIndexChanged, this, [this] {
        inputHistory_.clear();
        pressedHistory_.clear();
        restart();
    });
    connect(rate_, &QSpinBox::valueChanged, this, [this] {
        if (timer_->isActive())
            timer_->start(qMax(1, qRound(1000. / rate_->value())));
    });
    connect(fit, &QPushButton::clicked, this,
            [this] { view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio); });
    present();
}
void ScenePreview::stop() {
    seekTimer_->stop();
    seekTarget_ = -1;
    commandSeekWord_ = -1;
    timer_->stop();
    const QSignalBlocker block(play_);
    play_->setChecked(false);
    play_->setText("Play");
}
void ScenePreview::hideEvent(QHideEvent *event) {
    stop();
    cancelPositionPick();
    imageInput({});
    QWidget::hideEvent(event);
}
void ScenePreview::imageInput(LordlyImageInput input) {
    if (input.digital == liveInput_.digital && input.stick_x == liveInput_.stick_x &&
        input.stick_y == liveInput_.stick_y &&
        input.controller_present == liveInput_.controller_present)
        return;
    branchInput();
    liveInput_ = input;
}
void ScenePreview::branchInput() {
    if (seekTarget_ >= 0)
        stop();
    auto inputs = inputHistory_.lowerBound(tick_);
    while (inputs != inputHistory_.end())
        inputs = inputHistory_.erase(inputs);
    auto presses = pressedHistory_.lowerBound(tick_);
    while (presses != pressedHistory_.end())
        presses = pressedHistory_.erase(presses);
    auto snapshots = checkpoints_.upperBound(tick_);
    while (snapshots != checkpoints_.end())
        snapshots = checkpoints_.erase(snapshots);
    for (auto it = commandTimes_.begin(); it != commandTimes_.end();)
        if (it.value() > tick_) it = commandTimes_.erase(it); else ++it;
}
void ScenePreview::skipScene() {
    if (!current_ || current_->stopped || !current_->playback.skipArmed())
        return;
    branchInput();
    pendingPressed_ |= 0x1000;
    if (!timer_->isActive())
        step();
}
void ScenePreview::panelButton(quint16 bit) {
    if (!current_ || current_->stopped || (current_->playback.panels().empty() && !hasDialogue()))
        return;
    branchInput();
    pendingPressed_ |= bit;
    if (!timer_->isActive())
        step();
}
ScenePreview::~ScenePreview() {
    stop();
    // QWidget closes the focused child after derived members have been destroyed.
    for (auto field : dialogueNames_)
        disconnect(field, nullptr, this, nullptr);
}
void ScenePreview::setPortraitProvider(DialogueDrawing::PortraitProvider provider) {
    portraits_=std::move(provider);
    if (dialogueDrawing_)
        dialogueDrawing_->setPortraitProvider(portraits_);
    refreshPortraits();
}
void ScenePreview::setBodyAnimations(BodyAnimationData::Pointer animations) {
    bodyAnimations_ = std::move(animations);
    const auto classes = bodyAnimations_ ? bodyAnimations_->classes() : ClassData::load(rom_);
    classCount_ = int(classes.entries().size());
    bool removed = false;
    for (int i = 0; i < 5; ++i) {
        const QSignalBlocker primary(storyCast_[i]), secondary(storySecondary_[i]);
        for (int row = 0; row < storyCast_[i]->count(); ++row)
            storyCast_[i]->setItemText(
                row, decodeText(classes.entries()[storyCast_[i]->itemData(row).toInt()].name));
        const int prior = storySecondary_[i]->currentData().toInt();
        storySecondary_[i]->clear();
        for (int id = 1; id < classCount_; ++id)
            storySecondary_[i]->addItem(decodeText(classes.entries()[id].name), id);
        const int selected =
            prior > 0 && prior < classCount_ ? prior : storyCast_[i]->currentData().toInt();
        removed |= selected != prior;
        storySecondary_[i]->setCurrentIndex(storySecondary_[i]->findData(selected));
    }
    if (removed)
        castSource_->setText("A removed preview class was reset to its primary class");
    refreshPortraits();
}
void ScenePreview::setArtwork(const ActorArtworkEdits &edits) {
    renderer_.setArtwork(edits);
    spriteRenderer_.setArtwork(edits);
    stageRenderer_.setArtwork(edits);
    objectRenderer_.setArtwork(edits);
    refreshPortraits();
}
void ScenePreview::refreshPortraits() {
    if (current_) {
        const auto target = seekTarget_ >= 0 ? seekTarget_ : tick_;
        restart();
        seek(target);
    }
}
void ScenePreview::setDialogues(std::optional<Dialogues> dialogues) {
    if (dialogues.has_value() == dialogues_.has_value() &&
        (!dialogues || dialogues->json() == dialogues_->json()))
        return;
    dialogues_ = std::move(dialogues);
    inputHistory_.clear();
    pressedHistory_.clear();
    restart();
}
bool ScenePreview::hasDialogue() const {
    if (!current_ || !current_->playback.dialogue())
        return false;
    for (int i = 0; i < 6; ++i)
        if (current_->playback.dialogue()->resources.active(i))
            return true;
    return false;
}
std::optional<QPointF> ScenePreview::dialoguePoint(const Snapshot &snapshot, int slot) const {
    const auto &playback = snapshot.playback;
    if (!playback.actors().actor(slot))
        return std::nullopt;
    // Keep placement on the same current packed matrix path as the desktop Actor draw.
    // Prepare on a copy: placement must not advance drawing or publish linked-body changes.
    auto actors = playback.actors();
    if (mode_ == 0)
        actors.prepareModeZeroMatrices(playback.cameras().registered(), playback.transforms());
    else {
        if (!playback.lighting())
            throw Error("Dialogue world projection requires current lighting inputs");
        actors.prepareModeTwoMatrices(playback.cameras().actor(), *playback.lighting());
    }
    return playback.cameras()
        .actor()
        .projectPackedOrigin(actors.actor(slot)->record().mid(0xa0, 64))
        .screen;
}
void ScenePreview::imageButton(quint16 bit, bool down) {
    auto input = liveInput_;
    input.digital = down ? quint16(input.digital | bit) : quint16(input.digital & ~bit);
    imageInput(input);
    if (down && !timer_->isActive() && current_ && !current_->stopped &&
        current_->playback.imageControlsEnabled())
        step();
}
bool ScenePreview::eventFilter(QObject *object, QEvent *event) {
    if (draggingActor_ >= 0) {
        if ((object == view_ || object == view_->viewport()) && event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            cancelPositionPick();
            present();
            return true;
        }
        if (object == view_->viewport() && event->type() == QEvent::MouseMove) {
            const auto mouse = static_cast<QMouseEvent *>(event);
            const auto point = view_->mapToScene(mouse->position().toPoint());
            if (selectionPress_ && (mouse->position().toPoint() - *selectionPress_).manhattanLength() <
                                      QApplication::startDragDistance()) return true;
            try {
                const auto &playback = current_->playback;
                const auto position = playback.actors().positionAt(draggingActor_, dragOrigin_ + point - dragPress_,
                    playback.cameras(), playback.transforms(), playback.lighting());
                if (!std::isfinite(position.x) || !std::isfinite(position.z) ||
                    std::abs(position.x) > 2147483. || std::abs(position.z) > 2147483.)
                    throw Error("Actor position exceeds game coordinates");
                auto draft = *current_;
                const auto x = quint32(qint32(std::round(position.x * 1000.)));
                const auto z = quint32(qint32(std::round(position.z * 1000.)));
                int value = 0;
                const auto startX = x == quint32(-1000) && z == quint32(-1000) ? quint32(-999) : x;
                draft.playback.actors().service(7, {quint32(draggingActor_), startX, z, x, z, 0, 0}, value);
                render(draft, false);
                dragPosition_ = position;
                canvas_->clear(); selectionOutline_ = nullptr;
                canvas_->addPixmap(QPixmap::fromImage(draft.playback.display().present(draft.frame)));
                placementHelp_->setText(QString("Actor %1 · X %2 · Z %3. Release to place; Escape cancels.")
                    .arg(draggingActor_).arg(position.x, 0, 'f', 3).arg(position.z, 0, 'f', 3));
            } catch (const std::exception &e) {
                dragPosition_.reset();
                placementHelp_->setText(QString::fromUtf8(e.what()));
            }
            return true;
        }
        if (object == view_->viewport() && event->type() == QEvent::MouseButtonRelease) {
            const int slot = draggingActor_;
            const auto position = dragPosition_;
            cancelPositionPick();
            present();
            if (position) emit actorPlacedAtPlayhead(slot, *position);
            return true;
        }
    }
    if (placementCommand_ >= 0) {
        if ((object == view_ || object == view_->viewport()) && event->type() == QEvent::KeyPress &&
            static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            cancelPositionPick();
            return true;
        }
        if (object == view_->viewport() && (event->type() == QEvent::MouseButtonPress ||
                                            event->type() == QEvent::MouseButtonRelease)) {
            const auto mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() != Qt::LeftButton)
                return true;
            if (event->type() == QEvent::MouseButtonPress)
                return true;
            try {
                const auto point = view_->mapToScene(mouse->position().toPoint());
                if (!QRectF(0, 0, 320, 240).contains(point))
                    throw Error("Choose a point within the scene image.");
                const auto &playback = current_->playback;
                const auto position =
                    playback.actors().positionAt(placementSlot_, point, playback.cameras(),
                                                 playback.transforms(), playback.lighting());
                const int command = placementCommand_;
                cancelPositionPick();
                emit positionPicked(command, position);
            } catch (const std::exception &e) {
                placementHelp_->setText(QString::fromUtf8(e.what()));
            }
            return true;
        }
    }
    if (object == view_->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto mouse = static_cast<QMouseEvent *>(event);
            selectionPress_ =
                mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier
                    ? std::optional<QPoint>{mouse->position().toPoint()}
                    : std::nullopt;
            if (selectionPress_ && objectNavigationEnabled_ && current_ && !preparing_ &&
                current_->playback.director().selector() == selector_) {
                const auto point = view_->mapToScene(*selectionPress_);
                const auto target = objectAt({qFloor(point.x()), qFloor(point.y())});
                if (target && target.kind == SceneObjectKind::Actor) {
                    try {
                        const auto &playback = current_->playback;
                        const auto &origin = playback.actors().actor(target.slot)->pose().actor().position;
                        dragOrigin_ = playback.actors().projectPosition(target.slot, {origin.x, origin.y, origin.z},
                            playback.cameras(), playback.transforms(), playback.lighting());
                        stop();
                        selectedObject_ = target;
                        presentObjects();
                        dragPress_ = point; draggingActor_ = target.slot; dragPosition_.reset();
                        view_->setDragMode(QGraphicsView::NoDrag);
                        view_->setFocus();
                        return true;
                    } catch (const std::exception &e) {
                        placementHelp_->setText(QString::fromUtf8(e.what()));
                    }
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease ||
                   event->type() == QEvent::MouseButtonDblClick) {
            const auto mouse = static_cast<QMouseEvent *>(event);
            const bool doubleClick = event->type() == QEvent::MouseButtonDblClick;
            const bool click =
                doubleClick || (selectionPress_ &&
                                (mouse->position().toPoint() - *selectionPress_).manhattanLength() <
                                    QApplication::startDragDistance());
            selectionPress_.reset();
            if (click && mouse->button() == Qt::LeftButton &&
                mouse->modifiers() == Qt::NoModifier) {
                const auto point = view_->mapToScene(mouse->position().toPoint());
                const auto target = objectAt({qFloor(point.x()), qFloor(point.y())});
                if (target) {
                    stop();
                    selectedObject_ = target;
                    presentObjects();
                    if (doubleClick && objectNavigationEnabled_)
                        findSelectedAction();
                }
            }
        }
    }
    if (object == view_ && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this,
                           [this] { view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio); });
    if (object == view_ && event->type() == QEvent::KeyPress && current_ &&
        (!current_->playback.panels().empty() || hasDialogue())) {
        const auto key = static_cast<QKeyEvent *>(event);
        const quint16 bit = key->key() == Qt::Key_A      ? 0x8000
                            : key->key() == Qt::Key_B    ? 0x4000
                            : key->key() == Qt::Key_Up   ? 0x800
                            : key->key() == Qt::Key_Down ? 0x400
                                                         : 0;
        if (bit) {
            if (!key->isAutoRepeat())
                panelButton(bit);
            return true;
        }
    }
    if (object == view_ && event->type() == QEvent::KeyPress && current_ &&
        current_->playback.skipArmed()) {
        const auto key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (!key->isAutoRepeat())
                skipScene();
            return true;
        }
    }
    if (object == view_ &&
        (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) && current_ &&
        current_->playback.imageControlsEnabled()) {
        const auto key = static_cast<QKeyEvent *>(event);
        quint16 bit = 0;
        switch (key->key()) {
        case Qt::Key_Left:
            bit = 0x200;
            break;
        case Qt::Key_Right:
            bit = 0x100;
            break;
        case Qt::Key_Up:
            bit = 0x800;
            break;
        case Qt::Key_Down:
            bit = 0x400;
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            bit = 8;
            break;
        case Qt::Key_Minus:
            bit = 4;
            break;
        }
        if (bit) {
            if (!key->isAutoRepeat())
                imageButton(bit, event->type() == QEvent::KeyPress);
            return true;
        }
    }
    if (object == view_ && event->type() == QEvent::FocusOut)
        imageInput({});
    return QWidget::eventFilter(object, event);
}
void ScenePreview::selectScene(const ScenePrograms &programs, SceneId id, int mode,
                               std::optional<Dialogues> dialogues,
                               std::optional<SceneEvents> events) {
    sceneId_ = programs.canonical(id);
    playbackAssetId_ = id.authoredId.isEmpty() ? "rom-director:" + QString::number(id.nativeKey + 0x594284, 16).rightJustified(8, '0').toUpper() : "authored:" + id.authoredId;
    const auto selectors = programs.resource(id).selectors;
    const auto &catalog = SceneDocumentCompiler::catalog();
    if (id.authoredId.isEmpty()) for (auto it = catalog.begin(); it != catalog.end(); ++it) {
        const auto row = it.value().toObject();
        if (!selectors.isEmpty() && row["selectors"].toArray().contains(selectors.first())) {
            playbackAssetId_ = row["assetId"].toString(); break;
        }
    }
    cancelPositionPick();
    stop();
    dialogues_ = std::move(dialogues);
    inputHistory_.clear();
    pressedHistory_.clear();
    liveInput_ = {};
    pendingPressed_ = 0;
    launchImageUi_->hide();
    launchContextUi_->hide();
    storyCastEnabled_ = false;
    classDefaultsEnabled_ = false;
    party_.reset();
    storyCastUi_->hide();
    dialogueNamesUi_->hide();
    rewardUi_->hide();
    try {
        const auto resource = programs.resource(id);
        if (resource.selectors.isEmpty() || (mode != 0 && mode != 2))
            throw Error("Select a layered or world preview mode for this scene");
        resources_ = programs.runtimeDirectors();
        if (selector_ != resource.selectors.first()) selectedObject_ = {};
        selector_ = resource.selectors.first();
        mode_ = defaultMode_ = mode;
        const auto launch = SceneImages::launch(programs.program(id));
        const auto previousContext = launchContext_->currentData().toString();
        launchPlans_ = SceneLaunchPlan::choices(
            *resources_, events ? *events : SceneEvents::load(rom_), selector_);
        {
            const QSignalBlocker block(launchContext_);
            launchContext_->clear();
            for (const auto &plan : launchPlans_)
                launchContext_->addItem(plan.name, plan.id);
            const int previous = launchContext_->findData(previousContext);
            if (previous >= 0)
                launchContext_->setCurrentIndex(previous);
            launchContextUi_->setVisible(
                launchPlans_.size() > 1 ||
                (!launchPlans_.isEmpty() &&
                 (launchPlans_.first().steps.size() > 1 || launchPlans_.first().tailCall >= 0)));
        }
        storyCastEnabled_ = SceneStoryCast::supports(programs.program(id));
        if (mode == 2) party_ = SceneParty::infer(programs.program(id), classCount_);
        const auto actions = SceneActions::inspect(programs.program(id));
        classDefaultsEnabled_ = mode == 2 && std::any_of(actions.cbegin(), actions.cend(),
            [](const auto &action) { return action.name == "Create class Actor"; });
        storyCastUi_->setVisible(storyCastEnabled_);
        const auto &commands = programs.program(id).commands;
        dialogueNamesUi_->setVisible(std::any_of(commands.cbegin(), commands.cend(),
                                                 [](const auto &c) { return c.opcode == 0xbf; }));
        rewardUi_->setVisible(std::any_of(commands.cbegin(), commands.cend(), [](const auto &c) {
            return c.opcode == 0x62 && c.operands.size() == 2 && c.operands[1] == 34;
        }));
        {
            const QSignalBlocker block(launchImage_);
            launchImage_->clear();
            launchImage_->addItem(
                launch.image_row < 0
                    ? "Choose the event's launch image…"
                    : QString("Use scene selection (row %1)").arg(launch.image_row),
                -1);
            if (launch.resource_class == 4) {
                for (int row = 0; row < 69; ++row) {
                    try {
                        const auto image = SceneImages::presentationImage(rom_, row);
                        launchImage_->addItem(
                            QString("%1 · %2").arg(row).arg(SceneImages::catalog()[image].name),
                            row);
                    } catch (const Error &) {
                        // Some evolution rows select no supported image resource.
                    }
                }
                launchImageUi_->show();
            }
        }
        context_->setText(
            "Preview launch: flat terrain. Missing scene context pauses playback.\n"
            "World scenes use the common fresh-launch camera and paired background selectors.\n"
            "Scattered props use a fixed preview seed. Audio requests are displayed without sound.");
        if (storyCastEnabled_)
            context_->setText(context_->text() + "\nThis scene uses the selected preview class pairs. Its "
                                                 "authored positions stay unchanged; equipment uses class defaults.");
        if (classDefaultsEnabled_)
            context_->setText(context_->text() +
                "\nClass Actors preview with class-default equipment.");
        context_->setText(
            context_->text() +
            "\nDialogue uses the preview names and current Actor "
            "matrices. Desktop cadence: one resource pass per step, drawn over the scene.");
        if (selector_ == 0)
            context_->setText(
                context_->text() +
                "\nTitle reveal uses its separate update timing and resolution. Rectangle edge "
                "coverage is approximate; title menus and the People board remain unfinished.");
        contextSummary_ = context_->text();
        restart();
    } catch (const std::exception &e) {
        resources_.reset();
        current_.reset();
        checkpoints_.clear();
        frame_ = {};
        present();
        status_->setText(QString::fromUtf8(e.what()));
    }
}
void ScenePreview::setSaveProvider(std::function<const SaveDocument *()> provider) {
    saveProvider_ = std::move(provider);
    savedCast_->setEnabled(bool(saveProvider_));
    armyCast_->setEnabled(bool(saveProvider_));
}
void ScenePreview::loadArmyCast() {
    const auto save=saveProvider_ ? saveProvider_() : nullptr;
    if (!save) throw Error("Open a save in the Save workspace and select a slot first");
    const auto cast=SceneStoryCast::army(rom_,*save,classCount_);
    for (int i=0;i<5;++i) {
        const QSignalBlocker first(storyCast_[i]),second(storySecondary_[i]);
        storyCast_[i]->setCurrentIndex(storyCast_[i]->findData(cast[i].primary));
        storySecondary_[i]->setCurrentIndex(storySecondary_[i]->findData(cast[i].secondary));
    }
    const int slot=save->selectedSlot();
    castSource_->setText(slot<0 ? "Current army from the open runtime state"
                        : slot==15 ? "Current army from the suspend slot"
                        : QString("Current army from slot %1").arg(slot+1));
}
void ScenePreview::restart() {
    cancelPositionPick();
    stop();
    current_.reset();
    checkpoints_.clear();
    commandTimes_.clear();
    commandSeekNotice_.clear();
    deferredSeekTick_ = -1;
    tick_ = 0;
    preparing_ = false;
    liveInput_ = {};
    pendingPressed_ = 0;
    frame_ = {};
    const auto inputs = activeInputs();
    mode_ = inputs ? inputs->mode(defaultMode_) : defaultMode_;
    inputsStatus_->setText(inputs ? inputs->description() : QString{});
    clearInputs_->setEnabled(inputs != nullptr);
    if (!resources_) {
        present();
        return;
    }
    try {
        const auto &selectedProgram =
            resources_->entries()[resources_->selectors()[selector_]].program;
        const auto plan = inputs ? SceneLaunchPlan{"imported", "Imported playback inputs", {{selector_}}} :
            launchPlans_.value(launchContext_->currentIndex(),
                               SceneLaunchPlan{"direct", "Direct scene launch", {{selector_}}});
        const int startupSelector = plan.steps.first().selector;
        const auto &startupProgram =
            resources_->entries()[resources_->selectors()[startupSelector]].program;
        if (mode_ == 2)
            party_ = SceneParty::infer(startupProgram, classCount_);
        const bool inherited = plan.steps.size() > 1 || plan.tailCall >= 0;
        context_->setText(contextSummary_ + (party_ ? "\n" + party_->description() : QString{}) +
                          (inherited
                               ? "\nPreceding scenes supply shared Actors, dialogue and scenery. "
                                 "Event timing and setup acknowledgement are approximated."
                               : QString{}));
        for (quint32 opcode : {0x8fU, 0xbdU, 0xbeU}) {
            const bool used =
                std::any_of(plan.steps.cbegin(), plan.steps.cend(), [&](const auto &step) {
                    const auto &commands =
                        resources_->entries()[resources_->selectors()[step.selector]]
                            .program.commands;
                    return std::any_of(
                        commands.cbegin(), commands.cend(),
                        [=](const auto &command) { return command.opcode == opcode; });
                });
            if (used)
                context_->setText(
                    context_->text() +
                    (opcode == 0x8f ? "\nSaved story flags are clear in this preview; a saved game "
                                      "may choose different dialogue."
                     : opcode == 0xbd ? "\nAlternate presentation is inactive in this preview."
                     : plan.tailCall >= 0
                         ? "\nThe ending branch is selected to reach this scene."
                         : "\nChaos Frame uses the middle ending band for this preview."));
        }
        if (launchPlans_.isEmpty() && !SceneLaunchPlan::fresh(selectedProgram) &&
            (selectedProgram.resourceClass() == 1 || selectedProgram.resourceClass() == 8 ||
             selectedProgram.resourceClass() == 4 || selectedProgram.resourceClass() == 5))
            throw Error("No event predecessor supplies this scene's starting state. Assign it to "
                        "an event launch with the required context.");
        if (storyCastEnabled_ && armyCastActive_ && saveProvider_ && saveProvider_())
            loadArmyCast();
        auto camera = [](float fov, float x, float y, float z) {
            QByteArray data(56, 0);
            const float values[]{fov, 4.f / 3, 1, 20000, 1, x, y, z, 0, 0, 0, 0, 1, 0};
            for (int i = 0; i < 14; ++i)
                put32(data, i * 4, std::bit_cast<quint32>(values[i]));
            return SceneCamera(data, 1.f);
        };
        auto world = camera(45, 272, 80, 127);
        // The mode-zero shared-Actor producer uses game-to-world scale0.1
        // (native global801D06FC); the final screen camera remains independent.
        world = SceneCamera(world.bank(), .1f);
        auto cameras = mode_ ? SceneCameras::freshModeTwo(rom_)
                             : SceneCameras(camera(90, 0, 0, 120), world, world);
        ScenePlaybackStart start;
        start.bodyAnimations = bodyAnimations_;
        start.mode = mode_;
        start.sceneFlags = 0;
        start.lighting = LordlyActorLighting{{0, 0, 0}, {{200, -200, 50}, {-200, 200, 75}}};
        start.translations = plan.steps.last().translations;
        if (storyCastEnabled_ && !inputs) {
            auto cast = SceneStoryCast::sample();
            for (int i = 0; i < 5; ++i)
                cast[i] = {quint8(storyCast_[i]->currentData().toInt()),
                           quint8(storySecondary_[i]->currentData().toInt())};
            start.translations = SceneStoryCast::translations(cast);
        }
        if (inputs) inputs->configure(start, cameras);
        ScenePlayback playback(rom_, *resources_, selector_, start, cameras,
                               [](int, float, float) { return 0.f; });
        playback.supplyLaunchPlan(*resources_, plan);
        if (!inputs && (storyCastEnabled_ || classDefaultsEnabled_ || party_ || inherited))
            playback.actors().supplyBodyRows(QVector<QByteArray>(28, QByteArray(248, 0)));
        if (party_ && !inputs) party_->supply(playback.actors(),
                                   *(bodyAnimations_ ? bodyAnimations_ : BodyAnimationData::load(rom_)));
        std::array<QByteArray, 4> names;
        for (int i = 0; i < 4; ++i) {
            names[i] = dialogueNames_[i]->text().toLatin1();
            if (QString::fromLatin1(names[i]) != dialogueNames_[i]->text())
                throw Error("Preview names require supported native single-byte characters");
            names[i].append(char(0));
        }
        dialogueEnvironment_ = DialoguePlayback::environment(rom_);
        playback.supplyDialogue(SceneResources({1, -1, 0xffff}), SceneDialogue(0));
        playback.dialogue()->context = SceneDialogueContext::preview(std::move(names));
        // Silent preview has no resident voices and starts on route zero with unity buses.
        // Preserve typed requests for an eventual backend; never invent voice lifetimes.
        playback.supplyAudio(SceneAudio({0, {}, 0, 1}, {32767, 32767}));
        playback.panels().supplyReward(quint16(reward_->currentData().toUInt()));
        if (inputs) inputs->supply(playback);
        const auto launch = SceneImages::launch(startupProgram);
        if (launch.resource_class == 4) {
            const int overrideRow = launchImage_->currentData().toInt();
            const int row = overrideRow >= 0 ? overrideRow : launch.image_row;
            if (row < 0)
                throw Error("Choose the event's launch image to preview this scene. The scene does "
                            "not select an image itself.");
            playback.loadImagePresentation(row);
        }
        current_.emplace(Snapshot{std::move(playback), {}, {}, {}, false});
        if (inputs && !inputs->boundary().isEmpty()) {
            current_->stopped = true; current_->reason = inputs->boundary();
        }
        if (party_ && party_->preserved && !inputs)
            referenceCommand(*current_, 0x80000006U, {0});
        current_->dialogue.emplace(rom_, dialogues_);
        if (!dialogueDrawing_) {
            dialogueDrawing_.emplace(rom_);
            dialogueDrawing_->setPortraitProvider(portraits_);
        }
        render(*current_, false);
        checkpoints_.insert(0, *current_);
        present();
        view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
        if (inherited) {
            preparing_ = true;
            const QSignalBlocker block(play_);
            play_->setChecked(true);
            play_->setText("Pause setup");
            seekTimer_->start();
            present();
        }
    } catch (const std::exception &e) {
        current_.reset();
        present();
        status_->setText(QString::fromUtf8(e.what()));
    }
}
int ScenePreview::activeChannels(const Snapshot &next) const {
    if (next.playback.iris())
        return next.playback.iris()->count();
    int count = next.backgrounds ? next.backgrounds->channelCount() : 0;
    if (next.playback.imagePresentation())
        count = qMax(count, next.playback.imagePresentation()->channelCount());
    if (next.playback.imageControlsEnabled())
        count = qMax(count, 3);
    return count;
}
void ScenePreview::retainBackgrounds(Snapshot &next) {
    next.backgroundHandles = {};
    if (!next.backgrounds)
        return;
    for (int i = 0; i < next.backgrounds->channelCount(); ++i) {
        const auto &source = next.backgrounds->channel(i);
        if (source.records.isEmpty())
            continue;
        if (next.nextBackgroundHandle >= SceneIris::CapturedFrame)
            throw Error("Scene background resource identities are exhausted");
        const auto handle = next.nextBackgroundHandle++;
        next.backgroundResources.insert(handle, source);
        next.backgroundHandles[i] = handle;
    }
    next.playback.replaceIrisBackgrounds(next.backgroundHandles, next.backgrounds->channelCount());
}
bool ScenePreview::referenceCommand(Snapshot &next, quint32 opcode, const QVector<quint32> &args,
                                    int *value) {
    if ((opcode == 0x60 || opcode == 0x61) && args.size() == 2) {
        if (opcode == 0x60 && next.flatProps)
            next.flatProps->resetPose(qint32(args[0]), args[1]);
        else if (opcode == 0x61 && next.worldProps)
            next.worldProps->resetPose(qint32(args[0]), args[1]);
        else
            throw Error("Scene prop reset requires the current Stage objects");
        return true;
    }
    if (value && opcode == 0x8f && args.size() == 3) {
        *value = 0; // ROM-only preview has no saved persistent story-flag bank.
        return true;
    }
    // Declared isolated preview inputs. The gameplay owner is absent here.
    if (value && args.size() == 2 && (opcode == 0xbd || opcode == 0xbe)) {
        *value = opcode == 0xbe ? 1 : 0;
        return true;
    }
    if (opcode == 0x96 && args.size() == 1 && mode_ == 2 && party_ && !activeInputs()) {
        party_->supply(next.playback.actors(),
                       *(bodyAnimations_ ? bodyAnimations_ : BodyAnimationData::load(rom_)), true);
        return true;
    }
    if (opcode == 0x4d) {
        // Product capture timing: the current completed desktop draw, matching
        // the separate captured-frame transition's explicit framebuffer input.
        next.playback.startIris(args, next.frame, next.backgroundHandles, activeChannels(next));
        return true;
    }
    if (opcode == 0x73 && args.isEmpty()) {
        convertBackgrounds(next);
        // The installer resets the Stage caches, but does not run2A2BC8 on the
        // separate ordinary Actor cache. Existing Actor images keep their color.
        next.playback.effects().setSepia(true);
        return true;
    }
    if (opcode == 0x80000008U && args.size() == 2) {
        const int kind = quint8(args[0]);
        auto capture = next.playback.effects().capture();
        capture.start(args[0], args[1], next.frame);
        if (kind == 2) {
            convertBackgrounds(next);
            next.playback.effects().setSepia(true);
            next.playback.effects().images().convertAll();
        } else if (kind == 3) {
            next.playback.effects().setSepia(false);
            if (next.environmentSelector >= 0)
                referenceCommand(next, 0x80000006U, {quint32(next.environmentSelector)});
        }
        next.playback.effects().capture() = std::move(capture);
        return true;
    }
    if (opcode != 0x80000006 || args.size() != 1)
        return false;
    const int selector = args[0] == 0xffffffffU && mode_ == 2 ? 0 : qint32(args[0]);
    if (selector < 0)
        throw Error("This environment selector requires gameplay launch context");
    if (mode_ == 0) {
        auto backgrounds = SceneBackgroundRenderer::group(rom_, selector);
        if (next.playback.effects().sepia())
            backgrounds.convertToSepia();
        next.backgroundName = backgrounds.description();
        next.backgrounds = std::move(backgrounds);
        retainBackgrounds(next);
        next.environmentSelector = selector;
        return true;
    }
    // Common fresh mode-two launch copies environment into foreground selection.
    // Other launch paths must supply their separate foreground owner.
    auto backgrounds = SceneBackgroundRenderer::world(rom_, selector, selector);
    if (next.playback.effects().sepia())
        backgrounds.convertToSepia();
    const auto placements = StagePlacements::load(rom_, selector);
    auto randomState = next.previewRandom;
    auto random = [&] {
        // Stable desktop launch input, not a claim to reproduce the game's RNG state.
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState & 0x7fffU;
    };
    auto props = StageOrthographic::load(rom_, placements, random);
    auto objects = StagePerspective::load(rom_, placements, {255, 255, 255}, random);
    next.backgroundName = backgrounds.description();
    next.backgrounds = std::move(backgrounds);
    next.flatProps = std::move(props);
    next.worldProps = std::move(objects);
    next.foregroundSelector = selector;
    next.environmentSelector = selector;
    next.previewRandom = randomState;
    return true;
}
void ScenePreview::convertBackgrounds(Snapshot &next) {
    if (next.backgrounds)
        next.backgrounds->convertToSepia();
    QSet<quint32> current;
    for (int i = 0; i < 20; ++i)
        current.insert(next.playback.iris() ? next.playback.iris()->layers()[i].resource
                                            : next.backgroundHandles[i]);
    for (const auto handle : current)
        if (next.backgroundResources.contains(handle))
            SceneBackgroundRenderer::convertToSepia(next.backgroundResources[handle]);
}
void ScenePreview::render(Snapshot &next, bool commitDraw) {
    auto drawing = next.playback;
    if (commitDraw)
        drawing.display().submit();
    if (commitDraw && !drawing.titleScene())
        drawing.crop().draw();
    const bool sepia = drawing.effects().sepia();
    if (mode_ == 2 && commitDraw && !drawing.schedule().alternate && next.flatProps)
        next.flatProps->advance();
    if (mode_ == 2 && commitDraw && !drawing.schedule().alternate && next.worldProps)
        next.worldProps->advance();
    for (int slot = 0; slot < 30; ++slot)
        if (const auto sprite = drawing.sprites().sprite(slot)) {
            const auto animation = u16(sprite->record(), 0x44);
            if ((animation == 800 || animation == 801) &&
                (mode_ != 0 || !drawing.imagePresentation() ||
                 drawing.imagePresentation()->slot() < 0))
                throw Error("Special sprites require their effect projection owner");
        }
    const auto objects = next.worldProps ? next.worldProps->records() : QVector<QByteArray>{};
    RenderColor environment{0, 0, 0, 0};
    QImage frame(320, 240, QImage::Format_RGBA8888);
    frame.fill(Qt::transparent);
    SceneSelection selection;
    const int channelCount =
        drawing.iris()
            ? drawing.iris()->count()
            : qMax(next.backgrounds ? next.backgrounds->channelCount() : 20, activeChannels(next));
    if (mode_ == 0)
        drawing.prepareModeZeroDrawing(channelCount);
    else
        drawing.prepareModeTwoDrawing(*drawing.lighting());
    const auto queue = drawing.drawQueue(objects, {0x80300000, 0x80310000});
    {
        QPainter painter(&frame);
        auto overlay = [&](const QImage &image, double opacity = 1., bool coverSelection = true) {
            painter.save();
            painter.setOpacity(opacity);
            painter.drawImage(0, 0, image);
            painter.restore();
            if (coverSelection)
                selection.cover(image, opacity);
        };
        if (mode_ == 0) {
            const int count = channelCount;
            for (int channel = 0; channel < count; ++channel) {
                const auto parent = drawing.transforms().layerMatrix(channel);
                const auto &color = drawing.effects().colorRecord(channel);
                RenderColor backgroundColor{255, 255, 255, 255};
                if (!color.isEmpty())
                    for (int c = 0; c < 3; ++c)
                        backgroundColor[c] = u8(color, 4 + c);
                const bool presentation =
                    drawing.imagePresentation() && drawing.imagePresentation()->contains(channel);
                const auto resource = drawing.iris() ? drawing.iris()->layers()[channel].resource
                                                     : next.backgroundHandles[channel];
                const bool frozen = resource == SceneIris::CapturedFrame;
                if (presentation || frozen || next.backgroundResources.contains(resource)) {
                    QImage background(320, 240, QImage::Format_RGBA8888);
                    background.fill(Qt::transparent);
                    QVector<ActorRasterLayer> layers;
                    if (presentation)
                        layers = drawing.imagePresentation()->project(
                            channel, *drawing.imageView(), drawing.cameras().actor(), parent);
                    else {
                        const auto source = frozen
                                                ? SceneBackgroundChannel{"Captured frame",
                                                                         {{{-160, -120},
                                                                           drawing.iris()->frame(),
                                                                           {320, 240},
                                                                           2}}}
                                                : next.backgroundResources.value(resource);
                        layers = SceneBackgroundRenderer::project(
                            source, channel, drawing.cameras().actor(), parent, backgroundColor);
                    }
                    ActorRenderer::paint(background, layers);
                    selection.cover(background);
                    if (presentation) {
                        // ROM2B2868 pops the image/last trail matrix before
                        // calling29A930(1,1). Special sprites inherit this
                        // channel's parent, independently of image geometry.
                        const auto specialSprites = spriteRenderer_.project(
                            drawing.sprites(), 1, 1, drawing.cameras().actor(), parent,
                            {255, 255, 255}, drawing.effects().colorRecord(1), sepia);
                        ActorRenderer::paint(background, specialSprites);
                        selection.sprites(specialSprites);
                        if (!drawing.sprites().draws(1, 1).isEmpty())
                            environment = {0, 0, 0, 1};
                    }
                    overlay(background, 1., false);
                }
                const auto sprites = spriteRenderer_.project(drawing.sprites(), channel, 0,
                                                             drawing.cameras().actor(), parent,
                                                             {255, 255, 255}, color, sepia);
                if (!sprites.isEmpty()) {
                    QImage spriteFrame(320, 240, QImage::Format_RGBA8888);
                    spriteFrame.fill(Qt::transparent);
                    ActorRenderer::paint(spriteFrame, sprites);
                    selection.sprites(sprites);
                    overlay(spriteFrame, 1., false);
                }
                if (!drawing.sprites().draws(channel, 0).isEmpty())
                    environment = {0, 0, 0, 1};
                auto rendered = renderer_.layered(drawing.actors(), queue, 0, quint8(channel),
                                                  {drawing.cameras().actor(),
                                                   {255, 255, 255},
                                                   environment,
                                                   sepia,
                                                   &drawing.effects(),
                                                   {},
                                                   &selection});
                painter.drawImage(0, 0, rendered.image);
                environment = rendered.environment;
                if (drawing.iris() && !drawing.iris()->record().isEmpty() &&
                    u8(drawing.iris()->record(), 21) == channel)
                    overlay(drawing.iris()->render(rom_, drawing.cameras().actor(), parent));
            }
        } else {
            const auto camera = SceneCamera::orthographic(SceneWorldDrawing::clipMatrix());
            const auto parent = SceneWorldDrawing::modelMatrix(drawing.projection());
            auto tint = [&](WorldColorPass pass) { return drawing.effects().worldColor(pass); };
            auto drawFlat = [&](int pass) {
                QImage flat(320, 240, QImage::Format_RGBA8888);
                flat.fill(Qt::transparent);
                if (pass == 1 && next.flatProps)
                    ActorRenderer::paint(
                        flat, stageRenderer_.project(*next.flatProps, 1, camera, parent,
                                                     next.foregroundSelector, {255, 255, 255},
                                                     tint(WorldColorPass::BeforeForegroundProps),
                                                     false, environment, -5, -905, sepia));
                if (next.backgrounds)
                    ActorRenderer::paint(flat, next.backgrounds->project(
                                                   pass, camera, parent,
                                                   tint(pass ? WorldColorPass::Foreground
                                                             : WorldColorPass::Background)
                                                       .value_or(RenderColor{255, 255, 255, 255})));
                if (next.flatProps)
                    ActorRenderer::paint(flat, stageRenderer_.project(
                                                   *next.flatProps, pass ? 2 : 0, camera, parent,
                                                   next.foregroundSelector, {255, 255, 255},
                                                   tint(pass ? WorldColorPass::AfterForegroundProps
                                                             : WorldColorPass::BackgroundProps),
                                                   false, environment, -5, -905, sepia));
                selection.cover(flat);
                const auto sprites = spriteRenderer_.project(
                    drawing.sprites(), pass, 0, camera, parent, {255, 255, 255},
                    drawing.effects().colorRecord(pass), sepia);
                ActorRenderer::paint(flat, sprites);
                selection.sprites(sprites);
                overlay(flat, 1., false);
                if (!drawing.sprites().draws(pass, 0).isEmpty())
                    environment = {0, 0, 0, 1};
            };
            drawFlat(0);
            const auto rendered = renderer_.world(
                drawing.actors(), queue, objects, {96, 96}, {0, 0, 0, 0, 0, 0},
                {drawing.cameras().actor(),
                 {255, 255, 255},
                 environment,
                 sepia,
                 &drawing.effects(),
                 {},
                 &selection},
                [&](int, int index, QImage &image, ActorRenderState &state) {
                    if (!next.worldProps || index < 0 || index >= next.worldProps->props().size())
                        throw Error("Stage drawing refers to an unavailable object");
                    const auto layers = objectRenderer_.project(
                                   next.worldProps->props()[index], index,
                                   drawing.cameras().actor(), camera, parent, {255, 255, 255},
                                   state, tint(WorldColorPass::Objects), next.texturePhase, sepia);
                    ActorRenderer::paint(image, layers);
                    selection.cover(layers);
                });
            painter.drawImage(0, 0, rendered.image);
            environment = rendered.environment;
            drawFlat(1);
        }
        if (mode_ == 0 && !drawing.ribbons().empty())
            overlay(drawing.ribbons().render(drawing.cameras().actor(), *drawing.imageView()));
        if (drawing.effects().vignette()) {
            const auto camera = mode_ == 2
                                    ? SceneCamera::orthographic(SceneWorldDrawing::clipMatrix())
                                    : drawing.cameras().actor();
            const auto parent =
                mode_ == 2
                    ? SceneWorldDrawing::modelMatrix(SceneProjection{})
                    : drawing.transforms().layerMatrix(
                          (next.backgrounds ? qMax(1, next.backgrounds->channelCount()) : 20) - 1);
            overlay(SceneCapture::vignette(camera, parent));
        }
        if (mode_ == 2 && !drawing.effects().specialBlackRecord().isEmpty()) {
            const auto special =
                renderer_.special(drawing.actors(), drawing.effects(), {0, 0, 0, 0, 0, 0},
                                  {drawing.cameras().actor(), {255, 255, 255}, environment, sepia,
                                   nullptr, {}, &selection});
            painter.drawImage(0, 0, special.image);
            environment = special.environment;
        }
        const auto &capture = drawing.effects().capture();
        if ((capture.selector() == 2 || capture.selector() == 3) && !capture.frame().isNull()) {
            overlay(capture.frame(), capture.alpha() / 255.);
        }
        const auto clip = drawing.titleScene() ? frame.rect() : drawing.crop().clip();
        painter.save();
        painter.setClipRegion(QRegion(frame.rect()).subtracted(QRegion(clip)));
        painter.fillRect(frame.rect(), Qt::black);
        painter.restore();
        selection.clip(clip);
        if (!drawing.panels().empty())
            overlay(drawing.panels().render());
        if (drawing.titleScene())
            overlay(drawing.titleReveal().render(drawing.highResolution()));
        if (drawing.fade().present()) {
            const auto rgba = drawing.fade().rgba();
            painter.fillRect(frame.rect(), QColor(rgba[0], rgba[1], rgba[2], rgba[3]));
            selection.cover(rgba[3]);
        }
        if (next.dialogue && drawing.dialogue()) {
            const auto pixels =
                next.dialogue->image(drawing.dialogue()->resources, *dialogueDrawing_);
            const QImage dialogueImage(reinterpret_cast<const uchar *>(pixels.rgba.constData()),
                                 pixels.width, pixels.height, QImage::Format_RGBA8888);
            overlay(dialogueImage.copy()); // Retain owned pixels after the local buffer expires.
        }
    }
    next.frame = std::move(frame);
    next.selection = std::move(selection);
    QSet<quint32> retained;
    for (const auto handle : next.backgroundHandles)
        retained.insert(handle);
    if (drawing.iris())
        retained.unite(drawing.iris()->resources());
    for (auto it = next.backgroundResources.begin(); it != next.backgroundResources.end();)
        if (!retained.contains(it.key()))
            it = next.backgroundResources.erase(it);
        else
            ++it;
    if (commitDraw && mode_ == 2)
        ++next.texturePhase;
    if (commitDraw)
        drawing.effects().images().finishDraw();
    if (commitDraw) {
        if (mode_ == 0)
            drawing.finishImageDraw();
        drawing.display().complete();
        next.playback = std::move(drawing);
    }
}
void ScenePreview::step() {
    if (placementCommand_ >= 0) cancelPositionPick();
    if (!current_ || current_->stopped || tick_ >= 10000) {
        stop();
        return;
    }
    auto next = *current_;
    auto observedTimes = commandTimes_;
    try {
        if (preparing_ && tick_ % 45 == 0)
            pendingPressed_ |= 0x8000;
        if (!inputHistory_.contains(tick_))
            inputHistory_.insert(tick_, liveInput_);
        if (!pressedHistory_.contains(tick_))
            pressedHistory_.insert(tick_, pendingPressed_ | (activeInputs() ? activeInputs()->controller(tick_).value_or(0) : 0));
        next.playback.setImageInput(inputHistory_.value(tick_), pressedHistory_.value(tick_));
        pendingPressed_ = 0;
        next.playback.advanceFade();
        next.playback.audio()->clearRequests();
        next.playback.audio()->advance();
        // This reference stage starts these job/object pools empty. Commands
        // needing an unimplemented owner stop rather than adding hidden jobs.
        auto empty = [](LordlySceneService service) {
            switch (service) {
            case LORDLY_SCENE_MODE_ZERO_2:
            case LORDLY_SCENE_MODE_ZERO_4:
            case LORDLY_SCENE_MODE_ZERO_5:
            case LORDLY_SCENE_MODE_ZERO_6:
            case LORDLY_SCENE_AFTER_DIRECTOR:
            case LORDLY_SCENE_PATHS:
            case LORDLY_SCENE_DELAYED_CALLBACK:
                return true;
            default:
                return false;
            }
        };
        const auto result =
            // This callback exists only during this invocation, never in a retained snapshot.
            [&] {
                next.playback.dialogue()->project = [&](int slot) {
                    return dialoguePoint(next, slot);
                };
                const auto result = next.playback.advance(
                    empty, [&](quint32 op, const QVector<quint32> &args, int &value) {
                        return referenceCommand(next, op, args, &value);
                    }, 10000, 64, [&](quint32 key, int word, quint32 opcode) {
                        if (selector_ < 0 || key != resources_->selectors().value(selector_)) return;
                        if (!observedTimes.contains(word)) observedTimes[word] = preparing_ ? 0 : tick_ + 1;
                        next.activeWord = word;
                        if (opcode == 0x7d) {
                            next.exitWord = word;
                            next.unfinishedActors.clear(); next.pausedMovement = false;
                            for (int slot = 0; slot < 28; ++slot) {
                                const auto &motion = next.playback.actors().motion(slot);
                                if (next.playback.actors().actor(slot) && motion.present && motion.remaining) {
                                    next.unfinishedActors.insert(slot);
                                    next.pausedMovement |= motion.pause != 0;
                                }
                            }
                            next.unfinishedFade = next.playback.fade().slotActive() && next.playback.fade().remaining() > 0;
                            next.unfinishedPan = next.playback.projection().remaining();
                        }
                    });
                next.playback.dialogue()->project = {};
                return result;
            }();
        const bool complete =
            result == ScenePlayback::Advanced || result == ScenePlayback::Released;
        if (complete) {
            auto &state = *next.playback.dialogue();
            auto environment = dialogueEnvironment_;
            state.context->supplyTextSources(environment);
            const auto pressed = pressedHistory_.value(tick_);
            next.dialogue->advance(
                state.resources, state.windows, environment,
                {state.context->textParameter(), pressed, quint16(pressed & 0x0c00), 0});
        }
        render(next, complete);
        next.stopped = result != ScenePlayback::Advanced;
        next.reason = result == ScenePlayback::Released && next.playback.reason().isEmpty()
                          ? "Scene released." : next.playback.reason();
        current_ = std::move(next);
        commandTimes_ = std::move(observedTimes);
        ++tick_;
        if (tick_ % 120 == 0)
            checkpoints_.insert(tick_, *current_);
    } catch (const std::exception &e) {
        current_->stopped = true;
        current_->reason = QString::fromUtf8(e.what());
    }
    if (current_->stopped || tick_ == 10000)
        stop();
    present();
}
void ScenePreview::seek(int tick) {
    beginSeek(tick, -1);
}
void ScenePreview::seekToUpdate(int tick) {
    commandSeekNotice_.clear();
    if (preparing_) { commandSeekWord_ = -1; deferredSeekTick_ = tick; return; }
    beginSeek(tick, -1);
}
void ScenePreview::seekToCommand(int word) {
    commandSeekNotice_.clear();
    deferredSeekTick_ = -1;
    if (preparing_) {
        commandSeekWord_ = word;
        return;
    }
    if (commandTimes_.contains(word)) beginSeek(commandTimes_[word], -1);
    else beginSeek(10000, word);
}
void ScenePreview::beginSeek(int tick, int commandWord) {
    if (preparing_)
        return;
    cancelPositionPick();
    stop();
    if (!current_ || tick < 0 || tick > 10000)
        return;
    if (tick < tick_) {
        auto found = checkpoints_.upperBound(tick);
        if (found == checkpoints_.begin())
            return;
        --found;
        current_ = found.value();
        tick_ = found.key();
    }
    seekTarget_ = tick;
    commandSeekWord_ = commandWord;
    {
        const QSignalBlocker block(play_);
        play_->setChecked(true);
        play_->setText("Pause");
    }
    seekTimer_->start();
    continueSeek();
}
void ScenePreview::continueSeek() {
    if (preparing_) {
        if (!current_) {
            preparing_ = false;
            stop();
            return;
        }
        QElapsedTimer budget;
        budget.start();
        for (int i = 0; i < 4 && !current_->stopped && !current_->playback.selectedSceneEntered();
             ++i) {
            step();
            if (budget.elapsed() >= 8)
                break;
        }
        if (current_->playback.selectedSceneEntered()) {
            const int pendingCommand = commandSeekWord_;
            const int pendingTick = deferredSeekTick_;
            deferredSeekTick_ = -1;
            preparing_ = false;
            stop();
            tick_ = 0;
            inputHistory_.clear();
            pressedHistory_.clear();
            checkpoints_.clear();
            checkpoints_.insert(0, *current_);
            if (pendingCommand >= 0) { seekToCommand(pendingCommand); return; }
            if (pendingTick >= 0) { seekToUpdate(pendingTick); return; }
        } else if (current_->stopped || tick_ >= 10000) {
            preparing_ = false;
            current_->stopped = true;
            if (current_->reason.isEmpty())
                current_->reason = "The event predecessor did not reach this scene's entry.";
            stop();
        }
        present();
        return;
    }
    if (seekTarget_ < 0 || !current_)
        return;
    int target = seekTarget_;
    const int sought = commandSeekWord_;
    QElapsedTimer budget;
    budget.start();
    for (int i = 0; i < 4 && tick_ < target && !current_->stopped; ++i) {
        step();
        if (sought >= 0 && commandTimes_.contains(sought)) {
            target = seekTarget_ = commandTimes_[sought];
            commandSeekWord_ = -1;
            break;
        }
        if (budget.elapsed() >= 8)
            break;
    }
    if (tick_ >= target || current_->stopped) {
        if (sought >= 0 && !commandTimes_.contains(sought))
            commandSeekNotice_ = "That action was not reached on this playback branch.";
        stop();
    }
    present();
}
void ScenePreview::setObjectNavigationEnabled(bool enabled) {
    if (!enabled) cancelPositionPick();
    objectNavigationEnabled_ = enabled;
    findObject_->setVisible(enabled);
    presentSelectedObject();
}
void ScenePreview::cancelPositionPick() {
    draggingActor_ = -1; dragPosition_.reset(); selectionPress_.reset();
    placementCommand_ = placementSlot_ = -1;
    placementHelp_->clear(); cancelPlacement_->hide();
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->viewport()->unsetCursor();
}
void ScenePreview::beginPositionPick(int command, int slot) {
    cancelPositionPick();
    if (preparing_) {
        placementHelp_->setText(
            "Wait for the inherited scene to finish preparing before choosing a position.");
        return;
    }
    stop();
    try {
        if (!current_ || !objectNavigationEnabled_)
            throw Error("Load an editable scene before choosing an Actor position.");
        if (!current_->playback.actors().actor(slot) && !current_->stopped)
            step();
        const auto &playback = current_->playback;
        if (playback.director().selector() != selector_)
            throw Error("Playback is in a different scene. Return to the selected scene before "
                        "positioning its Actors.");
        if (!playback.actors().actor(slot))
            throw Error("Play the scene until this Actor is present, then choose its position.");
        const auto origin = playback.actors().actor(slot)->pose().actor().position;
        playback.actors().projectPosition(slot, {origin.x, origin.y, origin.z}, playback.cameras(),
                                          playback.transforms(), playback.lighting());
        placementCommand_ = command;
        placementSlot_ = slot;
        selectedObject_ = {SceneObjectKind::Actor, slot};
        presentObjects();
        placementHelp_->setText(
            "Click or drag to the Actor's new origin. Its height stays unchanged. Escape cancels.");
        cancelPlacement_->show();
        view_->setDragMode(QGraphicsView::NoDrag);
        view_->viewport()->setCursor(Qt::CrossCursor);
        view_->setFocus();
    } catch (const std::exception &e) {
        placementHelp_->setText(QString::fromUtf8(e.what()));
    }
}
SceneSelectionTarget ScenePreview::objectAt(QPoint point) const {
    return current_ && !current_->playback.display().black() ? current_->selection.objectAt(point)
                                                             : SceneSelectionTarget{};
}
int ScenePreview::actorAt(QPoint point) const {
    const auto target = objectAt(point);
    return target.kind == SceneObjectKind::Actor ? target.slot : -1;
}
int ScenePreview::spriteAt(QPoint point) const {
    const auto target = objectAt(point);
    return target.kind == SceneObjectKind::Sprite ? target.slot : -1;
}
void ScenePreview::findSelectedAction() {
    if (!current_ || !selectedObject_ || liveObjects_->currentIndex() < 0)
        return;
    const int selector = current_->playback.director().selector();
    if (selectedObject_.kind == SceneObjectKind::Actor)
        emit findActorAction(selector, selectedObject_.slot);
    else
        emit findSpriteAction(selector, selectedObject_.slot);
}
void ScenePreview::presentObjects() {
    QVector<SceneSelectionTarget> objects;
    if (current_) {
        for (int slot = 0; slot < 28; ++slot)
            if (current_->playback.actors().actor(slot))
                objects << SceneSelectionTarget{SceneObjectKind::Actor, slot};
        for (int slot = 0; slot < 30; ++slot)
            if (current_->playback.sprites().sprite(slot))
                objects << SceneSelectionTarget{SceneObjectKind::Sprite, slot};
    }
    bool changed = liveObjects_->count() != objects.size();
    for (int row = 0; !changed && row < objects.size(); ++row)
        changed = liveObjects_->itemData(row).toInt() != objects[row].slot ||
                  liveObjects_->itemData(row, Qt::UserRole + 1).toInt() != int(objects[row].kind);
    const QSignalBlocker guard(liveObjects_);
    if (changed) {
        liveObjects_->clear();
        for (const auto &object : objects) {
            liveObjects_->addItem(
                QString("%1 %2")
                    .arg(object.kind == SceneObjectKind::Actor ? "Actor" : "Sprite")
                    .arg(object.slot),
                object.slot);
            liveObjects_->setItemData(liveObjects_->count() - 1, int(object.kind),
                                      Qt::UserRole + 1);
        }
    }
    if (!selectedObject_ && !objects.isEmpty())
        selectedObject_ = objects.first();
    const int selected = int(objects.indexOf(selectedObject_));
    if (liveObjects_->currentIndex() != selected)
        liveObjects_->setCurrentIndex(selected);
    liveObjects_->setEnabled(!objects.isEmpty());
    presentSelectedObject();
}
void ScenePreview::presentSelectedObject() {
    const int slot = liveObjects_->currentIndex() < 0 ? -1 : selectedObject_.slot;
    const bool actor = selectedObject_.kind == SceneObjectKind::Actor;
    const bool present = current_ && slot >= 0 &&
                         (actor ? current_->playback.actors().actor(slot).has_value()
                                : current_->playback.sprites().sprite(slot) != nullptr);
    findObject_->setText(actor ? "Find Actor action" : "Find sprite action");
    findObject_->setEnabled(present && objectNavigationEnabled_ && !preparing_);
    presentSelectionOutline();
    if (!present) {
        objectState_->setText(!selectedObject_ ? "No active Actors or sprites"
                                               : QString("%1 %2 is not active at this step")
                                                     .arg(actor ? "Actor" : "Sprite")
                                                     .arg(selectedObject_.slot));
        return;
    }
    if (!actor) {
        const auto record = current_->playback.sprites().sprite(slot)->record();
        objectState_->setText(
            QString("Art bank %1 · Animation %2 · X %3 · Y %4 · Channel %5 · Scale %6%")
                .arg(qint32(u32(record, 0x30)))
                .arg(qint16(u16(record, 0x44)))
                .arg(qint32(u32(record, 0x24)))
                .arg(qint32(0U - u32(record, 0x28)))
                .arg(u8(record, 0x4a))
                .arg(qint16(u16(record, 0x48))));
        return;
    }
    const auto &instance = *current_->playback.actors().actor(slot);
    const auto record = instance.record();
    const auto source = qint32(u32(record, 0xe8)), owner = qint32(u32(record, 0xec));
    const auto kind = instance.pose().art().isBody()
                          ? QString("Class pair %1 / %2").arg(source).arg(owner)
                          : QString("Art bank %1").arg(source);
    objectState_->setText(QString("%1 · X %2 · Y %3 · Z %4")
                              .arg(kind)
                              .arg(std::bit_cast<float>(u32(record, 0x11c)), 0, 'f', 3)
                              .arg(std::bit_cast<float>(u32(record, 0x120)), 0, 'f', 3)
                              .arg(std::bit_cast<float>(u32(record, 0x124)), 0, 'f', 3));
}
void ScenePreview::presentSelectionOutline() {
    delete selectionOutline_;
    selectionOutline_ = nullptr;
    if (!current_ || current_->playback.display().black() || liveObjects_->currentIndex() < 0)
        return;
    const auto bounds = current_->selection.bounds(selectedObject_);
    if (bounds.isEmpty())
        return;
    QPen pen(QColor(selectedObject_.kind == SceneObjectKind::Actor ? "#54d6ff" : "#ffd166"));
    pen.setCosmetic(true);
    pen.setStyle(Qt::DashLine);
    selectionOutline_ = canvas_->addRect(bounds.adjusted(-1, -1, 1, 1), pen);
    selectionOutline_->setZValue(1);
    selectionOutline_->setAcceptedMouseButtons(Qt::NoButton);
}
void ScenePreview::present() {
    emit timelineChanged(sceneId_, commandTimes_, current_ ? current_->activeWord : -1, tick_);
    reportExit();
    const QSignalBlocker block(position_);
    position_->setValue(tick_);
    play_->setEnabled(current_ && !current_->stopped && tick_ < 10000);
    step_->setEnabled(play_->isEnabled() && !preparing_);
    position_->setEnabled(!preparing_);
    skip_->setVisible(current_ && current_->playback.skipArmed());
    skip_->setEnabled(play_->isEnabled());
    imageControlsUi_->setVisible(current_ && current_->playback.imageControlsEnabled());
    imageControlsUi_->setEnabled(play_->isEnabled());
    panelControlsUi_->setVisible(current_ &&
                                 (!current_->playback.panels().empty() || hasDialogue()));
    panelControlsUi_->setEnabled(play_->isEnabled());
    canvas_->clear();
    selectionOutline_ = nullptr;
    canvas_->setSceneRect(0, 0, 320, 240);
    setProperty("sceneTick", tick_);
    setProperty("scenePreparing", preparing_);
    setProperty("sceneStopped", !current_ || current_->stopped);
    setProperty("sceneAudioRequests", current_ && current_->playback.audio()
                                          ? qulonglong(current_->playback.audio()->requestCount())
                                          : qulonglong(0));
    setProperty("sceneDialogueState",
                current_ && current_->dialogue
                    ? current_->dialogue->state(0, current_->playback.dialogue()->resources)
                    : -1);
    if (!current_) {
        frame_ = {};
        status_->setText("No active scene.");
        presentObjects();
        return;
    }
    frame_ = current_->playback.display().present(current_->frame);
    canvas_->addPixmap(QPixmap::fromImage(frame_));
    presentObjects();

    const auto &director = current_->playback.director();
    status_->setText(QString("Step %1 · selector %2 · word %3%4%5")
                         .arg(tick_)
                         .arg(director.selector())
                         .arg(director.state().cursor)
                         .arg(current_->reason.isEmpty() ? QString{} : " · " + current_->reason)
                         .arg(current_->backgroundName.isEmpty()
                                  ? QString{}
                                  : "\nBackground channels: " + current_->backgroundName));
    if (current_->playback.display().black())
        status_->setText(status_->text() + "\nDisplay blacked out by scene command.");
    if (preparing_)
        status_->setText("Preparing inherited scene…\n" + status_->text());
    if (!commandSeekNotice_.isEmpty()) status_->setText(status_->text() + "\n" + commandSeekNotice_);
    if (!current_->playback.ribbons().empty())
        status_->setText(
            status_->text() +
            "\nPath ribbons: native curve and timing; approximate strip texture and edge drawing.");
    if (current_->playback.iris() && !current_->playback.iris()->record().isEmpty())
        status_->setText(status_->text() + "\nIris: native layers, timing and mesh; desktop "
                                           "capture and approximate edge blending.");
    if (!current_->playback.panels().empty())
        status_->setText(
            status_->text() +
            "\nPanels: native timing and layout; approximate frame and glyph rendering.");
    if (current_->playback.audio()->requestCount())
        status_->setText(
            status_->text() +
            QString("\nSilent audio preview: %1 native service requests; no resident voices.")
                .arg(current_->playback.audio()->requestCount()));
}
void ScenePreview::reportExit() {
    emit exitReached(sceneId_, current_ ? current_->exitWord : -1,
                     current_ ? current_->unfinishedActors : QSet<int>{},
                     current_ && current_->unfinishedFade, current_ ? current_->unfinishedPan : 0,
                     current_ && current_->pausedMovement);
}
const ScenePlaybackInputs *ScenePreview::activeInputs() const {
    const auto found = playbackInputs_.constFind(playbackAssetId_);
    return found == playbackInputs_.cend() ? nullptr : &found.value();
}
void ScenePreview::importPlaybackInputs(const QByteArray &bytes) {
    if (!resources_ || playbackAssetId_.isEmpty()) throw Error("Select a scene before importing playback inputs");
    const auto candidate = ScenePlaybackInputs::parse(bytes, playbackAssetId_);
    const auto launch = candidate.known("externalProducers").toObject()["directorLaunch"].toObject();
    if (!launch.isEmpty() && resources_->selectors().value(launch["selector"].toInt(-1)) != resources_->selectors()[selector_])
        throw Error("Playback launch selector does not identify the selected scene");
    const auto previousInputs = playbackInputs_;
    const auto previous = current_;
    const auto checkpoints = checkpoints_;
    const auto times = commandTimes_;
    const auto inputHistory = inputHistory_;
    const auto pressedHistory = pressedHistory_;
    const int tick = tick_, mode = mode_;
    const bool preparing = preparing_;
    playbackInputs_.insert(playbackAssetId_, candidate);
    inputHistory_.clear(); pressedHistory_.clear();
    restart();
    if (!current_) {
        const auto reason = status_->text();
        playbackInputs_ = previousInputs; current_ = previous; checkpoints_ = checkpoints;
        commandTimes_ = times; inputHistory_ = inputHistory; pressedHistory_ = pressedHistory;
        tick_ = tick; mode_ = mode; preparing_ = preparing;
        inputsStatus_->setText(activeInputs() ? activeInputs()->description() : QString{});
        clearInputs_->setEnabled(activeInputs() != nullptr);
        present();
        if (preparing) seekTimer_->start();
        throw Error(reason);
    }
}
void ScenePreview::clearPlaybackInputs() {
    playbackInputs_.remove(playbackAssetId_);
    inputHistory_.clear(); pressedHistory_.clear();
    restart();
}
} // namespace lordly
