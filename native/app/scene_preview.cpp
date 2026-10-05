#include "app/scene_preview.h"
#include "engine/scene_images.h"
#include <QElapsedTimer>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHideEvent>
#include <QLabel>
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
    context_->setWordWrap(true);
    layout->addWidget(context_);
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
    controls->addWidget(reset);
    controls->addWidget(new QLabel("Step"));
    controls->addWidget(position_);
    controls->addWidget(rate_);
    controls->addStretch();
    controls->addWidget(fit);
    layout->addLayout(controls);
    canvas_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(canvas_);
    view_->setObjectName("sceneCanvas");
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
    connect(position_, &QSpinBox::valueChanged, this, &ScenePreview::seek);
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
    QWidget::hideEvent(event);
}
void ScenePreview::selectScene(const ScenePrograms &programs, SceneId id, int mode) {
    stop();
    try {
        const auto resource = programs.resource(id);
        if (resource.selectors.isEmpty() || (mode != 0 && mode != 2))
            throw Error("Select a layered or world preview mode for this scene");
        resources_ = programs.runtimeDirectors();
        selector_ = resource.selectors.first();
        mode_ = mode;
        context_->setText(
            "Preview launch: flat terrain and no army roster. Missing services pause playback.\n"
            "World scenes use the common fresh-launch camera and paired background selectors.\n"
            "Scattered props use a fixed preview seed. Equipment, special sprites and audio remain "
            "unfinished.");
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
void ScenePreview::restart() {
    stop();
    current_.reset();
    checkpoints_.clear();
    tick_ = 0;
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
        start.mode = mode_;
        start.sceneFlags = 0;
        start.lighting = LordlyActorLighting{{0, 0, 0}, {{200, -200, 50}, {-200, 200, 75}}};
        ScenePlayback playback(rom_, *resources_, selector_, start, cameras,
                               [](int, float, float) { return 0.f; });
        current_.emplace(Snapshot{std::move(playback), {}, {}, {}, false});
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
bool ScenePreview::referenceCommand(Snapshot &next, quint32 opcode, const QVector<quint32> &args) {
    if (opcode != 0x80000006 || args.size() != 1)
        return false;
    const int selector = qint32(args[0]);
    if (selector < 0)
        throw Error("This environment selector requires gameplay launch context");
    if (mode_ == 0) {
        auto backgrounds = SceneBackgroundRenderer::group(rom_, selector);
        next.backgroundName = backgrounds.description();
        next.backgrounds = std::move(backgrounds);
        return true;
    }
    // Common fresh mode-two launch copies environment into foreground selection.
    // Other launch paths must supply their separate foreground owner.
    auto backgrounds = SceneBackgroundRenderer::world(rom_, selector, selector);
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
    next.previewRandom = randomState;
    return true;
}
void ScenePreview::render(Snapshot &next, bool commitDraw) {
    auto drawing = next.playback;
    if (mode_ == 2 && commitDraw && !drawing.schedule().alternate && next.flatProps)
        next.flatProps->advance();
    if (mode_ == 2 && commitDraw && !drawing.schedule().alternate && next.worldProps)
        next.worldProps->advance();
    for (int slot = 0; slot < 30; ++slot)
        if (const auto sprite = drawing.sprites().sprite(slot)) {
            const auto animation = u16(sprite->record(), 0x44);
            if (animation == 800 || animation == 801)
                throw Error("Special sprites require their effect projection owner");
        }
    const auto objects = next.worldProps ? next.worldProps->records() : QVector<QByteArray>{};
    const auto queue = drawing.drawQueue(objects, {0x80300000, 0x80310000});
    RenderColor environment{0, 0, 0, 0};
    QImage frame(320, 240, QImage::Format_RGBA8888);
    frame.fill(Qt::transparent);
    if (mode_ == 0)
        drawing.prepareModeZeroDrawing(next.backgrounds ? next.backgrounds->channelCount() : 20);
    else
        drawing.prepareModeTwoDrawing(*drawing.lighting());
    {
        QPainter painter(&frame);
        if (mode_ == 0) {
            const int count = next.backgrounds ? next.backgrounds->channelCount() : 20;
            for (int channel = 0; channel < count; ++channel) {
                const auto parent = drawing.transforms().layerMatrix(channel);
                if (next.backgrounds) {
                    QImage background(320, 240, QImage::Format_RGBA8888);
                    background.fill(Qt::transparent);
                    ActorRenderer::paint(
                        background,
                        next.backgrounds->project(channel, drawing.cameras().actor(), parent));
                    painter.drawImage(0, 0, background);
                }
                const auto sprites =
                    spriteRenderer_.project(drawing.sprites(), channel, 0,
                                            drawing.cameras().actor(), parent, {255, 255, 255});
                if (!sprites.isEmpty()) {
                    QImage spriteFrame(320, 240, QImage::Format_RGBA8888);
                    spriteFrame.fill(Qt::transparent);
                    ActorRenderer::paint(spriteFrame, sprites);
                    painter.drawImage(0, 0, spriteFrame);
                }
                if (!drawing.sprites().draws(channel, 0).isEmpty())
                    environment = {0, 0, 0, 1};
                auto rendered =
                    renderer_.layered(drawing.actors(), queue, 0, quint8(channel),
                                      {drawing.cameras().actor(), {255, 255, 255}, environment});
                painter.drawImage(0, 0, rendered.image);
                environment = rendered.environment;
            }
        } else {
            const auto camera = SceneCamera::orthographic(SceneWorldDrawing::clipMatrix());
            const auto parent = SceneWorldDrawing::modelMatrix(drawing.projection());
            auto drawFlat = [&](int pass) {
                QImage flat(320, 240, QImage::Format_RGBA8888);
                flat.fill(Qt::transparent);
                if (pass == 1 && next.flatProps)
                    ActorRenderer::paint(
                        flat, stageRenderer_.project(*next.flatProps, 1, camera, parent,
                                                     next.foregroundSelector, {255, 255, 255}, {},
                                                     false, environment));
                if (next.backgrounds)
                    ActorRenderer::paint(flat, next.backgrounds->project(pass, camera, parent));
                if (next.flatProps)
                    ActorRenderer::paint(
                        flat, stageRenderer_.project(*next.flatProps, pass ? 2 : 0, camera, parent,
                                                     next.foregroundSelector, {255, 255, 255}, {},
                                                     false, environment));
                ActorRenderer::paint(flat,
                                     spriteRenderer_.project(drawing.sprites(), pass, 0, camera,
                                                             parent, {255, 255, 255}));
                painter.drawImage(0, 0, flat);
                if (!drawing.sprites().draws(pass, 0).isEmpty())
                    environment = {0, 0, 0, 1};
            };
            drawFlat(0);
            const auto rendered = renderer_.world(
                drawing.actors(), queue, objects, {96, 96}, {0, 0, 0, 0, 0, 0},
                {drawing.cameras().actor(), {255, 255, 255}, environment},
                [&](int, int index, QImage &image, ActorRenderState &state) {
                    if (!next.worldProps || index < 0 || index >= next.worldProps->props().size())
                        throw Error("Stage drawing refers to an unavailable object");
                    ActorRenderer::paint(image, objectRenderer_.project(
                                                    next.worldProps->props()[index], index,
                                                    drawing.cameras().actor(), camera, parent,
                                                    {255, 255, 255}, state, {}, next.texturePhase));
                });
            painter.drawImage(0, 0, rendered.image);
            environment = rendered.environment;
            drawFlat(1);
        }
        if (drawing.fade().present()) {
            const auto rgba = drawing.fade().rgba();
            painter.fillRect(frame.rect(), QColor(rgba[0], rgba[1], rgba[2], rgba[3]));
        }
    }
    next.frame = std::move(frame);
    if (commitDraw && mode_ == 2)
        ++next.texturePhase;
    if (commitDraw)
        next.playback = std::move(drawing);
}
void ScenePreview::step() {
    if (!current_ || current_->stopped || tick_ >= 10000) {
        stop();
        return;
    }
    auto next = *current_;
    try {
        next.playback.advanceFade();
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
            case LORDLY_SCENE_TRANSITION:
            case LORDLY_SCENE_DELAYED_CALLBACK:
                return true;
            default:
                return false;
            }
        };
        const auto result =
            next.playback.advance(empty, [&](quint32 op, const QVector<quint32> &args, int &) {
                return referenceCommand(next, op, args);
            });
        const bool complete =
            result == ScenePlayback::Advanced || result == ScenePlayback::Released;
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
void ScenePreview::present() {
    const QSignalBlocker block(position_);
    position_->setValue(tick_);
    play_->setEnabled(current_ && !current_->stopped && tick_ < 10000);
    step_->setEnabled(play_->isEnabled());
    canvas_->clear();
    canvas_->setSceneRect(0, 0, 320, 240);
    setProperty("sceneTick", tick_);
    setProperty("sceneStopped", !current_ || current_->stopped);
    if (!current_) {
        frame_ = {};
        status_->setText("No active scene.");
        return;
    }
    frame_ = current_->frame;
    canvas_->addPixmap(QPixmap::fromImage(frame_));
    const auto &director = current_->playback.director();
    status_->setText(QString("Step %1 · selector %2 · word %3%4%5")
                         .arg(tick_)
                         .arg(director.selector())
                         .arg(director.state().cursor)
                         .arg(current_->reason.isEmpty() ? QString{} : " · " + current_->reason)
                         .arg(current_->backgroundName.isEmpty()
                                  ? QString{}
                                  : "\nBackground channels: " + current_->backgroundName));
}
} // namespace lordly
