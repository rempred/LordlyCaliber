#include "app/scene_preview.h"
#include "engine/class_data.h"
#include "engine/save_document.h"
#include "engine/scene_images.h"
#include "engine/scene_actions.h"
#include "engine/scene_story_cast.h"
#include "engine/text.h"
#include <QComboBox>
#include <QApplication>
#include <QElapsedTimer>
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
    savedCast_ = new QPushButton("Use open save");
    savedCast_->setObjectName("sceneUseSavedCast");
    savedCast_->setEnabled(false);
    savedCast_->setToolTip("Use the five class pairs stored in the Save workspace's selected slot. "
                           "This does not rerun the story event's army selection.");
    auto resetCast = new QPushButton("Use sample cast");
    resetCast->setObjectName("sceneResetCast");
    castTools->addWidget(savedCast_);
    castTools->addWidget(resetCast);
    castTools->addWidget(castSource_, 1);
    castRow->addLayout(castTools, 2, 0, 1, 6);
    auto applyCast = [this](const std::array<StoryCastMember, 5> &cast, const QString &source) {
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
    actors->addWidget(new QLabel("Live Actor"));
    liveActors_ = new QComboBox;
    liveActors_->setObjectName("sceneLiveActors");
    liveActors_->setMinimumContentsLength(10);
    actors->addWidget(liveActors_);
    actorState_ = new QLabel("No active Actors");
    actorState_->setObjectName("sceneActorState");
    actorState_->setWordWrap(true);
    actors->addWidget(actorState_, 1);
    findActor_ = new QPushButton("Find Actor action");
    findActor_->setObjectName("sceneFindActorAction");
    findActor_->setToolTip("Find the selected slot's placement or first supported action in the "
                           "current script. Conditions may give a slot more than one action.");
    findActor_->setEnabled(false);
    findActor_->hide();
    actors->addWidget(findActor_);
    layout->addLayout(actors);
    connect(liveActors_, &QComboBox::currentIndexChanged, this, [this] {
        if (liveActors_->currentIndex() >= 0)
            selectedActorSlot_ = liveActors_->currentData().toInt();
        presentSelectedActor();
    });
    connect(findActor_, &QPushButton::clicked, this, [this] {
        if (!current_ || liveActors_->currentIndex() < 0)
            return;
        stop();
        emit findActorAction(current_->playback.director().selector(),
                             liveActors_->currentData().toInt());
    });
    connect(position_, &QSpinBox::valueChanged, this, &ScenePreview::seek);
    connect(launchImage_, &QComboBox::currentIndexChanged, this, [this] {
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
    timer_->stop();
    const QSignalBlocker block(play_);
    play_->setChecked(false);
    play_->setText("Play");
}
void ScenePreview::hideEvent(QHideEvent *event) {
    stop();
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
    if (object == view_->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            const auto mouse = static_cast<QMouseEvent *>(event);
            actorPress_ = mouse->button() == Qt::LeftButton && mouse->modifiers() == Qt::NoModifier
                              ? std::optional<QPoint>{mouse->position().toPoint()}
                              : std::nullopt;
        } else if (event->type() == QEvent::MouseButtonRelease ||
                   event->type() == QEvent::MouseButtonDblClick) {
            const auto mouse = static_cast<QMouseEvent *>(event);
            const bool doubleClick = event->type() == QEvent::MouseButtonDblClick;
            const bool click =
                doubleClick ||
                (actorPress_ && (mouse->position().toPoint() - *actorPress_).manhattanLength() <
                                    QApplication::startDragDistance());
            actorPress_.reset();
            if (click && mouse->button() == Qt::LeftButton &&
                mouse->modifiers() == Qt::NoModifier) {
                const auto point = view_->mapToScene(mouse->position().toPoint());
                const int slot = actorAt({qFloor(point.x()), qFloor(point.y())});
                if (slot >= 0) {
                    stop();
                    selectedActorSlot_ = slot;
                    presentActors();
                    if (doubleClick && actorNavigationEnabled_)
                        emit findActorAction(current_->playback.director().selector(), slot);
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
                               std::optional<Dialogues> dialogues) {
    stop();
    dialogues_ = std::move(dialogues);
    inputHistory_.clear();
    pressedHistory_.clear();
    liveInput_ = {};
    pendingPressed_ = 0;
    launchImageUi_->hide();
    storyCastEnabled_ = false;
    classDefaultsEnabled_ = false;
    storyCastUi_->hide();
    dialogueNamesUi_->hide();
    rewardUi_->hide();
    try {
        const auto resource = programs.resource(id);
        if (resource.selectors.isEmpty() || (mode != 0 && mode != 2))
            throw Error("Select a layered or world preview mode for this scene");
        resources_ = programs.runtimeDirectors();
        if (selector_ != resource.selectors.first()) selectedActorSlot_ = -1;
        selector_ = resource.selectors.first();
        mode_ = mode;
        const auto launch = SceneImages::launch(programs.program(id));
        storyCastEnabled_ = SceneStoryCast::supports(programs.program(id));
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
            "Preview launch: flat terrain and no army roster. Missing services pause playback.\n"
            "World scenes use the common fresh-launch camera and paired background selectors.\n"
            "Scattered props use a fixed preview seed. Launch bindings and audio remain "
            "unfinished.");
        if (storyCastEnabled_)
            context_->setText(context_->text() + "\nThis scene uses the selected preview class pairs. Its "
                                                 "authored positions stay unchanged.");
        if (classDefaultsEnabled_)
            context_->setText(context_->text() +
                "\nClass Actors preview with class-default equipment. Army bindings still require launch data.");
        context_->setText(
            context_->text() +
            "\nDialogue uses the preview names and current Actor "
            "matrices. Desktop cadence: one resource pass per step, drawn over the scene.");
        if (selector_ == 0)
            context_->setText(
                context_->text() +
                "\nTitle reveal uses its separate update timing and resolution. Rectangle edge "
                "coverage is approximate; title menus and the People board remain unfinished.");
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
}
void ScenePreview::restart() {
    stop();
    current_.reset();
    checkpoints_.clear();
    tick_ = 0;
    liveInput_ = {};
    pendingPressed_ = 0;
    frame_ = {};
    if (!resources_) {
        present();
        return;
    }
    try {
        auto camera = [](float fov, float x, float y, float z) {
            QByteArray data(56, 0);
            const float values[]{fov, 4.f / 3, 1, 20000, 1, x, y, z, 0, 0, 0, 0, 1, 0};
            for (int i = 0; i < 14; ++i)
                put32(data, i * 4, std::bit_cast<quint32>(values[i]));
            return SceneCamera(data, 1.f);
        };
        auto world = camera(45, 272, 80, 127);
        auto cameras = mode_ ? SceneCameras::freshModeTwo(rom_)
                             : SceneCameras(camera(90, 0, 0, 120), world, world);
        ScenePlaybackStart start;
        start.bodyAnimations = bodyAnimations_;
        start.mode = mode_;
        start.sceneFlags = 0;
        start.lighting = LordlyActorLighting{{0, 0, 0}, {{200, -200, 50}, {-200, 200, 75}}};
        if (storyCastEnabled_) {
            auto cast = SceneStoryCast::sample();
            for (int i = 0; i < 5; ++i)
                cast[i] = {quint8(storyCast_[i]->currentData().toInt()),
                           quint8(storySecondary_[i]->currentData().toInt())};
            start.translations = SceneStoryCast::translations(cast);
        }
        ScenePlayback playback(rom_, *resources_, selector_, start, cameras,
                               [](int, float, float) { return 0.f; });
        if (storyCastEnabled_ || classDefaultsEnabled_)
            playback.actors().supplyBodyRows(QVector<QByteArray>(28, QByteArray(248, 0)));
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
        const auto launch =
            SceneImages::launch(resources_->entries()[resources_->selectors()[selector_]].program);
        if (launch.resource_class == 4) {
            const int overrideRow = launchImage_->currentData().toInt();
            const int row = overrideRow >= 0 ? overrideRow : launch.image_row;
            if (row < 0)
                throw Error("Choose the event's launch image to preview this scene. The scene does "
                            "not select an image itself.");
            playback.loadImagePresentation(row);
        }
        current_.emplace(Snapshot{std::move(playback), {}, {}, {}, false});
        current_->dialogue.emplace(rom_, dialogues_);
        if (!dialogueDrawing_) {
            dialogueDrawing_.emplace(rom_);
            dialogueDrawing_->setPortraitProvider(portraits_);
        }
        render(*current_, false);
        checkpoints_.insert(0, *current_);
        present();
        view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
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
bool ScenePreview::referenceCommand(Snapshot &next, quint32 opcode, const QVector<quint32> &args) {
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
    const int selector = qint32(args[0]);
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
        auto overlay = [&](const QImage &image, double opacity = 1.) {
            painter.save();
            painter.setOpacity(opacity);
            painter.drawImage(0, 0, image);
            painter.restore();
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
                    if (presentation) {
                        // ROM2B2868 pops the image/last trail matrix before
                        // calling29A930(1,1). Special sprites inherit this
                        // channel's parent, independently of image geometry.
                        ActorRenderer::paint(background,
                                             spriteRenderer_.project(
                                                 drawing.sprites(), 1, 1, drawing.cameras().actor(),
                                                 parent, {255, 255, 255},
                                                 drawing.effects().colorRecord(1), sepia));
                        if (!drawing.sprites().draws(1, 1).isEmpty())
                            environment = {0, 0, 0, 1};
                    }
                    overlay(background);
                }
                const auto sprites = spriteRenderer_.project(drawing.sprites(), channel, 0,
                                                             drawing.cameras().actor(), parent,
                                                             {255, 255, 255}, color, sepia);
                if (!sprites.isEmpty()) {
                    QImage spriteFrame(320, 240, QImage::Format_RGBA8888);
                    spriteFrame.fill(Qt::transparent);
                    ActorRenderer::paint(spriteFrame, sprites);
                    overlay(spriteFrame);
                }
                if (!drawing.sprites().draws(channel, 0).isEmpty())
                    environment = {0, 0, 0, 1};
                auto rendered = renderer_.layered(drawing.actors(), queue, 0, quint8(channel),
                                                  {drawing.cameras().actor(),
                                                   {255, 255, 255},
                                                   environment,
                                                   sepia,
                                                   &drawing.effects(), {}, &selection});
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
                ActorRenderer::paint(
                    flat, spriteRenderer_.project(drawing.sprites(), pass, 0, camera, parent,
                                                  {255, 255, 255},
                                                  drawing.effects().colorRecord(pass), sepia));
                overlay(flat);
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
    if (!current_ || current_->stopped || tick_ >= 10000) {
        stop();
        return;
    }
    auto next = *current_;
    try {
        if (!inputHistory_.contains(tick_))
            inputHistory_.insert(tick_, liveInput_);
        if (!pressedHistory_.contains(tick_))
            pressedHistory_.insert(tick_, pendingPressed_);
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
                    empty, [&](quint32 op, const QVector<quint32> &args, int &) {
                        return referenceCommand(next, op, args);
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
        next.reason =
            result == ScenePlayback::Released ? "Scene released." : next.playback.reason();
        current_ = std::move(next);
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
    {
        const QSignalBlocker block(play_);
        play_->setChecked(true);
        play_->setText("Pause");
    }
    seekTimer_->start();
    continueSeek();
}
void ScenePreview::continueSeek() {
    if (seekTarget_ < 0 || !current_)
        return;
    const int target = seekTarget_;
    QElapsedTimer budget;
    budget.start();
    for (int i = 0; i < 4 && tick_ < target && !current_->stopped; ++i) {
        step();
        if (budget.elapsed() >= 8)
            break;
    }
    if (tick_ >= target || current_->stopped)
        stop();
    present();
}
void ScenePreview::setActorNavigationEnabled(bool enabled) {
    actorNavigationEnabled_ = enabled;
    findActor_->setVisible(enabled);
    presentSelectedActor();
}
int ScenePreview::actorAt(QPoint point) const {
    return current_ && !current_->playback.display().black() ? current_->selection.actorAt(point) : -1;
}
void ScenePreview::presentActors() {
    QVector<int> actorSlots;
    if (current_)
        for (int slot = 0; slot < 28; ++slot)
            if (current_->playback.actors().actor(slot))
                actorSlots << slot;
    bool changed = liveActors_->count() != actorSlots.size();
    if (!changed)
        for (int row = 0; row < actorSlots.size(); ++row)
            if (liveActors_->itemData(row).toInt() != actorSlots[row]) {
                changed = true;
                break;
            }
    const QSignalBlocker guard(liveActors_);
    if (changed) {
        liveActors_->clear();
        for (int slot : actorSlots)
            liveActors_->addItem(QString("Actor %1").arg(slot), slot);
    }
    if (selectedActorSlot_ < 0 && liveActors_->count())
        selectedActorSlot_ = liveActors_->itemData(0).toInt();
    const int selected = liveActors_->findData(selectedActorSlot_);
    if (liveActors_->currentIndex() != selected)
        liveActors_->setCurrentIndex(selected);
    liveActors_->setEnabled(liveActors_->count() > 0);
    presentSelectedActor();
}
void ScenePreview::presentSelectedActor() {
    const int slot = liveActors_->currentIndex() < 0 ? -1 : liveActors_->currentData().toInt();
    const bool present =
        current_ && slot >= 0 && current_->playback.actors().actor(slot).has_value();
    findActor_->setEnabled(present && actorNavigationEnabled_);
    presentActorOutline();
    if (!present) {
        actorState_->setText(
            selectedActorSlot_ < 0
                ? "No active Actors"
                : QString("Actor %1 is not active at this step").arg(selectedActorSlot_));
        return;
    }
    const auto &actor = *current_->playback.actors().actor(slot);
    const auto record = actor.record();
    const auto source = qint32(u32(record, 0xe8)), owner = qint32(u32(record, 0xec));
    const auto kind = actor.pose().art().isBody()
                          ? QString("Class pair %1 / %2").arg(source).arg(owner)
                          : QString("Art bank %1").arg(source);
    actorState_->setText(QString("%1 · X %2 · Y %3 · Z %4")
                             .arg(kind)
                             .arg(std::bit_cast<float>(u32(record, 0x11c)), 0, 'f', 3)
                             .arg(std::bit_cast<float>(u32(record, 0x120)), 0, 'f', 3)
                             .arg(std::bit_cast<float>(u32(record, 0x124)), 0, 'f', 3));
}
void ScenePreview::presentActorOutline() {
    delete actorOutline_;
    actorOutline_ = nullptr;
    if (!current_ || current_->playback.display().black() || liveActors_->currentIndex() < 0)
        return;
    const auto bounds = current_->selection.actorBounds(liveActors_->currentData().toInt());
    if (bounds.isEmpty())
        return;
    QPen pen(QColor("#54d6ff"));
    pen.setCosmetic(true);
    pen.setStyle(Qt::DashLine);
    actorOutline_ = canvas_->addRect(bounds.adjusted(-1, -1, 1, 1), pen);
    actorOutline_->setZValue(1);
    actorOutline_->setAcceptedMouseButtons(Qt::NoButton);
}
void ScenePreview::present() {
    const QSignalBlocker block(position_);
    position_->setValue(tick_);
    play_->setEnabled(current_ && !current_->stopped && tick_ < 10000);
    step_->setEnabled(play_->isEnabled());
    skip_->setVisible(current_ && current_->playback.skipArmed());
    skip_->setEnabled(play_->isEnabled());
    imageControlsUi_->setVisible(current_ && current_->playback.imageControlsEnabled());
    imageControlsUi_->setEnabled(play_->isEnabled());
    panelControlsUi_->setVisible(current_ &&
                                 (!current_->playback.panels().empty() || hasDialogue()));
    panelControlsUi_->setEnabled(play_->isEnabled());
    canvas_->clear();
    actorOutline_ = nullptr;
    canvas_->setSceneRect(0, 0, 320, 240);
    setProperty("sceneTick", tick_);
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
        presentActors();
        return;
    }
    frame_ = current_->playback.display().present(current_->frame);
    canvas_->addPixmap(QPixmap::fromImage(frame_));
    presentActors();

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
} // namespace lordly
