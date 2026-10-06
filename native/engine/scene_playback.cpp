#include "engine/scene_playback.h"
namespace lordly {
ScenePlayback::ScenePlayback(const Rom &rom, Directors resources, int selector,
                             ScenePlaybackStart start, SceneCameras cameras,
                             ActorInstance::Height height)
    : director_(std::move(resources), selector, start.mode, std::move(start.translations)),
      rom_(rom), actors_(rom, start.mode, start.presentation, std::move(height), std::move(start.bodyAnimations)), sprites_(rom),
      cameras_(std::move(cameras)), panels_(rom), imageMotion_(rom), ribbons_(rom),
      lighting_(start.lighting) {
    schedule_.mode = quint8(start.mode);
    screenVariant_ = selector;
    schedule_.alternate = start.alternate;
    schedule_.delayed_counter = start.delayedCounter;
    if (start.sceneFlags)
        actors_.supplySceneFlags(*start.sceneFlags);
}
void ScenePlayback::supplyImageView(QByteArray context) {
    if (context.size() != 0x848)
        throw Error("Image presentation requires its complete context");
    imageView_ = std::move(context);
}
void ScenePlayback::loadImagePresentation(int row) {
    auto image = SceneImagePresentation::load(rom_, row);
    auto context = image.initialContext();
    imagePresentation_ = std::move(image);
    imageView_ = std::move(context);
}
void ScenePlayback::setMode(int mode) {
    if (mode < 0 || mode > 2)
        throw Error("Scene mode is outside 0..2");
    director_.setMode(mode);
    actors_.setMode(mode);
    schedule_.mode = quint8(mode);
}
void ScenePlayback::syncIris() {
    if (!iris_)
        return;
    for (int i = 0; i < 20; ++i) {
        auto &layer = iris_->layers()[i];
        layer.record = transforms_.channel(i);
        const auto &images = imagePresentation_->channels();
        if (images.contains(i))
            layer.resource = images[i] ? 2 : 1;
        else if (layer.resource == 1 || layer.resource == 2)
            layer.resource = 0;
    }
}
void ScenePlayback::applyIris() {
    QMap<int, bool> images;
    for (int i = 0; i < 20; ++i) {
        const auto &layer = iris_->layers()[i];
        transforms_.supplyChannel(i, layer.record);
        if (layer.resource == 1 || layer.resource == 2)
            images[i] = layer.resource == 2;
    }
    imagePresentation_->assignChannels(std::move(images));
    put8(*imageView_, 0x838, quint8(iris_->selected()));
}
void ScenePlayback::startIris(const QVector<quint32> &args, const QImage &frame,
                              const std::array<quint32, 20> &backgrounds, int channelCount) {
    if (schedule_.mode != 0 || !imagePresentation_ || !imageView_ || imagePresentation_->slot() < 0)
        throw Error("Iris requires a constructed layered image presentation");
    auto next = *this;
    if (!next.iris_ || next.iris_->record().isEmpty()) {
        SceneIris::Layers layers;
        for (int i = 0; i < 20; ++i) {
            if ((backgrounds[i] && backgrounds[i] < 3) ||
                backgrounds[i] == SceneIris::CapturedFrame)
                throw Error("Background handle overlaps an image identity");
            layers[i] = {backgrounds[i], transforms_.channel(i)};
        }
        next.iris_.emplace(channelCount, u8(*imageView_, 0x838), std::move(layers));
    }
    next.syncIris();
    QVector<quint8> actors(28, 0), sprites(30, 0);
    for (int i = 0; i < 28; ++i)
        if (const auto &actor = actors_.actor(i))
            actors[i] = u8(actor->record(), 0x13e);
    for (int i = 0; i < 30; ++i)
        if (const auto *sprite = sprites_.sprite(i))
            sprites[i] = u8(sprite->record(), 0x4a);
    next.iris_->start(args, frame, actors, sprites);
    int value = 0;
    for (int i = 0; i < 28; ++i)
        if (actors_.actor(i))
            next.actors_.service(0x1c, {quint32(i), actors[i]}, value);
    for (int i = 0; i < 30; ++i)
        next.sprites_.setPass(i, sprites[i]);
    next.applyIris();
    actors_ = std::move(next.actors_);
    sprites_ = std::move(next.sprites_);
    transforms_ = std::move(next.transforms_);
    iris_ = std::move(next.iris_);
    imagePresentation_ = std::move(next.imagePresentation_);
    imageView_ = std::move(next.imageView_);
}
void ScenePlayback::replaceIrisBackgrounds(const std::array<quint32, 20> &handles, int count) {
    if (!iris_)
        return;
    if (iris_->record().isEmpty()) {
        iris_.reset();
        return;
    }
    iris_->replaceBackgrounds(handles, count);
    applyIris();
}
bool ScenePlayback::command(quint32 opcode, const QVector<quint32> &args, int &value) {
    if (opcode == 0x84) {
        if (!display_.blackout())
            throw Error("Display blackout is waiting for submitted graphics tasks to finish");
        return true;
    }
    if (opcode == 0x85) {
        display_.requestReveal();
        return true;
    }
    if (titleScene() && (opcode == 0x62 || opcode == 0x6a || opcode == 0x6b))
        throw Error("Title menu panels require their separate early-overlay owner");
    if (opcode == 0x62) {
        if (args.size() != 2)
            throw Error("Panel creation requires two operands");
        if (args[1] == 33 && !imageView_)
            throw Error("Map help requires its launch image dimensions");
        panels_.create(qint32(args[0]), qint32(args[1]),
                       imageView_ ? qint16(u16(*imageView_, 0x834)) : 320,
                       imageView_ ? qint16(u16(*imageView_, 0x836)) : 240);
        return true;
    }
    if (opcode == 0x6a) {
        if (args.size() != 3)
            throw Error("Panel query requires three operands");
        value = panels_.query(qint32(args[2]));
        return true;
    }
    if (opcode == 0x6b) {
        if (args.size() != 1)
            throw Error("Panel release requires one operand");
        panels_.release(qint32(args[0]));
        return true;
    }
    if (opcode == 0x3b || opcode == 0x3c) {
        if (titleScene()) {
            if (opcode == 0x3b)
                titleReveal_.start(args);
            else
                value = titleReveal_.active();
            return true;
        }
        if (opcode == 0x3b)
            crop_.start(args);
        else
            value = crop_.active();
        return true;
    }
    if (titleScene() && (opcode == 0xb7 || opcode == 0xb8)) {
        highResolution_ = opcode == 0xb8;
        return true;
    }
    if (opcode == 0x4e) {
        value = iris_ && iris_->active();
        return true;
    }
    if (opcode == 0xb2 || opcode == 0xb3) {
        if (imageView_) {
            const quint8 state = opcode == 0xb2 ? 1 : 0;
            put8(*imageView_, 0x840, state);
            director_.setBarrierState(state);
        }
        return true;
    }
    if (opcode >= 0x49 && opcode <= 0x4c) {
        if (!imageView_)
            throw Error("Map ribbons require their launch image presentation");
        QMap<int, QPointF> markers;
        if (opcode == 0x49)
            for (int i = 0; i < 30; ++i)
                if (const auto &sprite = sprites_.sprite(i)) {
                    const auto &r = sprite->record();
                    markers[i] = QPointF(float(qint32(u32(r, 0x24))), float(qint32(u32(r, 0x28))));
                }
        return ribbons_.service(opcode, args, value, markers);
    }
    if (opcode == 0x55 || opcode == 0x6c || opcode == 0x6d) {
        if (!imageView_ || !imagePresentation_)
            throw Error("Image controls require the launch image presentation");
        if (opcode == 0x6c) {
            if (args.size() != 1)
                throw Error("Image arrow selection requires one operand");
            imageControls_.enable(args[0], *imageView_, sprites_);
        } else if (opcode == 0x6d)
            imageControls_.disable(*imageView_, sprites_);
        else {
            auto channel = transforms_.channel(1);
            imageMotion_.manual(*imageView_, channel, imageInput_);
            transforms_.supplyChannel(1, channel);
        }
        return true;
    }
    if (opcode == 0x50) {
        if (!imagePresentation_ || !imageView_)
            throw Error("Image transition requires its launch image presentation");
        auto channel = transforms_.channel(1);
        imagePresentation_->startEcho(args, *imageView_, channel);
        transforms_.supplyChannel(1, channel);
        return true;
    }
    if (opcode == 0x54) {
        if (!imageView_)
            throw Error("Image transition query requires its launch image presentation");
        value = qint32(
            lordly_image_echo_query(reinterpret_cast<const uint8_t *>(imageView_->constData())));
        return true;
    }
    if (opcode == 0x3a) {
        if (!imagePresentation_ || !imageView_)
            throw Error("Scene vignette requires its launch image presentation");
        imagePresentation_->construct(rom_, args, cameras_.registered(), *imageView_);
        return true;
    }
    if (opcode == 0x76) {
        if (!imageView_)
            throw Error("Scripted image motion requires its launch image presentation");
        imageMotion_.start(args, *imageView_, transforms_.channel(1));
        return true;
    }
    if (opcode == 0x77) {
        value = imageMotion_.active();
        return true;
    }
    if (opcode == 0x47 && lighting_) {
        if (args.size() != 3)
            throw Error("Scene lighting command is truncated");
        // Retained func_00284288 case47 casts signed words directly, without
        // the thousandths conversion used for Actor/camera coordinates.
        for (int i = 0; i < 3; ++i)
            lighting_->point[i] = float(qint32(args[i]));
        if (schedule_.mode == 2) {
            lighting_->planar[0][0] = lighting_->point[0];
            lighting_->planar[0][1] = lighting_->point[2];
            lighting_->planar[0][2] = lighting_->point[1];
        }
        return true;
    }
    if (schedule_.mode == 0 && (opcode == 8 || opcode == 0xd)) {
        if (opcode == 8) {
            if (args.size() != 3)
                throw Error("Scene transform command is truncated");
            transforms_.start(rom_, qint32(args[0]), qint32(args[1]), qint32(args[2]));
        } else
            value = transforms_.done();
        return true;
    }
    if (dialogue_) {
        auto &state = *dialogue_;
        switch (opcode) {
        case 0xbf: {
            if (args.size() != 13)
                throw Error("Dialogue creation command is truncated");
            if (!state.context)
                throw Error("Dialogue creation requires current roster and text context");
            auto next = state;
            const auto &actor = actors_.actor(qint32(args[3]));
            const auto record = actor ? actor->record() : QByteArray{};
            QByteArray row;
            if (schedule_.mode == 2 && actor && u8(record, 0x147) < 20)
                row = actors_.bodyRow(u8(record, 0x147));
            next.context->prepare(schedule_.mode, record, row);
            auto portrait = next.portrait;
            if (!portrait)
                portrait = [this](int slot) { return actors_.dialoguePortrait(slot); };
            next.windows.create(args, next.resources, next.project, portrait);
            state = std::move(next);
            return true;
        }
        case 0x1a:
            if (!state.context)
                throw Error("Dialogue text parameter requires current shared context");
            if (args.size() != 1)
                throw Error("Dialogue text parameter command is truncated");
            state.context->setTextParameter(args[0]);
            return true;
        case 5:
        case 6:
            if (args.size() != 1)
                throw Error("Dialogue window command is truncated");
            if (opcode == 5)
                state.windows.close(qint32(args[0]), state.resources);
            else
                state.windows.resume(args[0], state.resources);
            return true;
        case 0x10:
        case 0x25:
            if (args.size() != 3)
                throw Error("Dialogue query is truncated");
            value = opcode == 0x10 ? state.windows.query(args[2], state.resources)
                                   : state.windows.control(qint32(args[2]));
            return true;
        default:
            break;
        }
    }
    if (opcode == 0x97) {
        if (!schedule_.alternate)
            schedule_.alternate = 1;
        return true;
    }
    if (opcode == 0x80) {
        if (args.size() != 1)
            throw Error("Actor color selection command is truncated");
        effects_.selectColorActor(args[0], actors_.actorRecords());
        return true;
    }
    if (opcode == 0x39 && args.size() == 1 && args[0] != 0) {
        schedule_.alternate = 0;
        return true;
    }
    if (opcode == 0x63) {
        // This callback is consumed synchronously. Copies/checkpoints retain no
        // pointer to an earlier playback owner or its path records.
        return sprites_.service(
            opcode, args, value,
            [this](qint32 group, qint32 entry, int slot, const SpritePosePlayer &prepared) {
                QMap<int, QPointF> markers;
                if (group != -1) {
                    for (int i = 0; i < 30; ++i)
                        if (const auto sprite = sprites_.sprite(i)) {
                            const auto record = sprite->record();
                            markers[i] = QPointF(float(qint32(u32(record, 0x24))),
                                                 float(qint32(u32(record, 0x28))));
                        }
                    // Native creation precedes path lookup; a linked point can
                    // reference this new/replaced sprite, including immediate pose motion.
                    const auto record = prepared.record();
                    markers[slot] =
                        QPointF(float(qint32(u32(record, 0x24))), float(qint32(u32(record, 0x28))));
                }
                return ribbons_.rotation(group, entry, markers);
            });
    }
    if ((audio_ && audio_->service(opcode, args)) || actors_.service(opcode, args, value) ||
        sprites_.service(opcode, args, value) ||
        projection_.service(schedule_.mode, opcode, args) ||
        cameras_.service(schedule_.mode, opcode, args) || fade_.service(opcode, args, value) ||
        effects_.service(opcode, args))
        return true;
    return commands_ && *commands_ && (*commands_)(opcode, args, value);
}
bool ScenePlayback::update(LordlySceneService service) {
    switch (service) {
    case LORDLY_SCENE_TRANSITION:
        effects_.capture().advance();
        return true;
    case LORDLY_SCENE_COLOR:
        effects_.advanceColors();
        return true;
    case LORDLY_SCENE_SPECIAL_BLACK:
        effects_.advanceSpecialBlack();
        return true;
    case LORDLY_SCENE_MODE_TWO_PREPARE:
        // In the mode-two image ROM 2AB53C calls 2AB558, an empty 28-iteration
        // loop with no scene reads/writes. It is not the mode-zero divider.
        return true;
    case LORDLY_SCENE_MODE_ZERO_3:
        actors_.advanceDivider();
        return true;
    case LORDLY_SCENE_MODE_ZERO_1:
        transforms_.advance();
        return true;
    case LORDLY_SCENE_MODE_ZERO_2:
        if (imagePresentation_ && imageView_)
            imagePresentation_->advanceEcho(rom_, *imageView_);
        return true;
    case LORDLY_SCENE_MODE_ZERO_6: {
        // 2B529C updates the image trails and twenty path ribbons. It returns
        // immediately when the oversized-image owner is absent.
        if (imageView_) {
            ribbons_.advance();
            const LordlyImageEcho empty{};
            lordly_image_echo_fade(reinterpret_cast<uint8_t *>(imageView_->data()),
                                   imagePresentation_ ? &imagePresentation_->echo() : &empty,
                                   pressed_);
        }
        return true;
    }
    case LORDLY_SCENE_MODE_ZERO_5:
        if (iris_) {
            syncIris();
            if (iris_->advance()) {
                for (int i = 0; i < 28; ++i)
                    actors_.vacate(i);
                imageControls_.releaseAttached(sprites_);
                int unused = 0;
                sprites_.service(0x66, {0xffffffffU}, unused);
            }
            applyIris();
        }
        return true;
    case LORDLY_SCENE_MODE_ZERO_4:
        if (imageMotion_.active()) {
            if (!imageView_)
                throw Error("Image motion lost its presentation context");
            auto channel = transforms_.channel(1);
            imageMotion_.advance(*imageView_, channel);
            transforms_.supplyChannel(1, channel);
        }
        return true;
    case LORDLY_SCENE_RESOURCE_CACHE:
        // Recovered func_002071F4 (5ae68e14) only ages/releases native allocation
        // handles. ActorArt/ActorPosePlayer own those decoded resources on PC;
        // Qt shared ownership replaces the target's twenty-record eviction pool.
        return true;
    case LORDLY_SCENE_COUNTER:
        director_.advanceCounter();
        return true;
    case LORDLY_SCENE_HEIGHT:
        actors_.advanceHeights();
        return true;
    case LORDLY_SCENE_MOVEMENT:
        actors_.advanceMovement();
        return true;
    case LORDLY_SCENE_FACING:
        actors_.advanceTurns();
        return true;
    case LORDLY_SCENE_PROJECTION:
        projection_.advance();
        return true;
    case LORDLY_SCENE_MODE_TWO_4:
        actors_.advancePresentation();
        return true;
    case LORDLY_SCENE_AFTER_DIRECTOR:
        if (!dialogue_)
            return supplemental_ && *supplemental_ && (*supplemental_)(service);
        dialogue_->windows.updatePlacement(dialogue_->resources, dialogue_->project);
        return true;
    case LORDLY_SCENE_POSES:
        if (!poseSweepStarted_) {
            // The original sweep rechecks alternate at entry. A preceding
            // service can change it even during a normal scheduler invocation.
            if (schedule_.alternate)
                return true;
            poseSweepStarted_ = true;
        }
        if (!actorsAdvanced_) {
            if (!actors_.advancePoses(instructionBudget_)) {
                poseBudgetReached_ = true;
                reason_ = "Actor pose instruction budget reached";
                return false;
            }
            actorsAdvanced_ = true;
        }
        if (!sprites_.advancePoses(instructionBudget_)) {
            poseBudgetReached_ = true;
            reason_ = "Sprite pose instruction budget reached";
            return false;
        }
        actorsAdvanced_ = false;
        poseSweepStarted_ = false;
        return true;
    case LORDLY_SCENE_DIRECTOR:
        director_.setBarrierState(imageView_ ? u8(*imageView_, 0x840) : 0);
        director_.setController(pressed_);
        directorResult_ =
            director_.dispatch([this](quint32 op, const QVector<quint32> &args,
                                      int &value) { return command(op, args, value); },
                               instructionBudget_);
        if (imageView_)
            put8(*imageView_, 0x840, director_.state().barrier_state);
        if (directorResult_ == LORDLY_DIRECTOR_RELEASE)
            releaseRequested_ = true;
        if (directorResult_ == LORDLY_DIRECTOR_YIELD || directorResult_ == LORDLY_DIRECTOR_END ||
            directorResult_ == LORDLY_DIRECTOR_RELEASE)
            return true;
        reason_ = director_.reason();
        return false;
    default:
        return supplemental_ && *supplemental_ && (*supplemental_)(service);
    }
}
int ScenePlayback::invoke(void *opaque, LordlySceneService service) {
    auto &self = *static_cast<ScenePlayback *>(opaque);
    self.pending_ = service;
    try {
        if (self.update(service)) {
            self.pending_.reset();
            return 1;
        }
    } catch (const std::exception &e) {
        self.reason_ = QString::fromUtf8(e.what());
    }
    if (self.reason_.isEmpty())
        self.reason_ = QString("Scene service at RAM %1 requires its current domain state")
                           .arg(lordly_scene_service_address(service), 8, 16, QChar('0'));
    return 0;
}
ScenePlayback::Result ScenePlayback::advance(const Update &supplemental,
                                             const DirectorSession::Service &commands,
                                             int instructionBudget, int stageBudget) {
    if (supplemental_ || commands_)
        throw Error("A scene update is already in progress");
    if (instructionBudget < 1 || stageBudget < 1)
        throw Error("Scene budgets must be positive");
    if (released_)
        return Released;
    reason_.clear();
    poseBudgetReached_ = false;
    pending_.reset();
    supplemental_ = &supplemental;
    commands_ = &commands;
    instructionBudget_ = instructionBudget;
    int status;
    if (titleScene()) {
        // Early wrapper ROM281C78..281CE4 updates counter, Director, title menu,
        // People board, then this reveal. Its scheduler is not the Actor sweep.
        // The two independent menu owners still reject their launch commands;
        // neither exists in this playback until its domain is implemented.
        status = LORDLY_SCENE_COMPLETE;
        int stages = 0;
        while (titlePhase_ < 3) {
            if (stages == stageBudget) {
                status = LORDLY_SCENE_BUDGET;
                break;
            }
            if (titlePhase_ == 2)
                titleReveal_.advance();
            else if (!invoke(this,
                             titlePhase_ == 0 ? LORDLY_SCENE_COUNTER : LORDLY_SCENE_DIRECTOR)) {
                status = LORDLY_SCENE_UNAVAILABLE;
                break;
            }
            ++titlePhase_;
            ++stages;
        }
        if (status == LORDLY_SCENE_COMPLETE)
            titlePhase_ = 0;
    } else
        status = lordly_scene_schedule(&schedule_, invoke, this, stageBudget);
    supplemental_ = nullptr;
    commands_ = nullptr;
    if (status == LORDLY_SCENE_COMPLETE) {
        // Scene wrapper ROM281A8C calls the menu updater after the complete
        // Actor/Director scheduler, including its alternate path.
        if (!titleScene())
            panels_.advance(pressed_, pressed_ & 0x0c00);
        ++completed_;
        released_ = releaseRequested_;
        return released_ ? Released : Advanced;
    }
    if (status == LORDLY_SCENE_INVALID)
        throw Error("Scene scheduler continuation is invalid");
    if (status == LORDLY_SCENE_BUDGET || poseBudgetReached_ ||
        (pending_ == LORDLY_SCENE_DIRECTOR && directorResult_ == LORDLY_DIRECTOR_BUDGET)) {
        if (reason_.isEmpty())
            reason_ = "Scene update budget reached";
        return Budget;
    }
    return Suspended;
}
void ScenePlayback::advanceFade() {
    if (fade_.advance())
        fade_.cleanup();
}
void ScenePlayback::prepareModeZeroDrawing(int channelCount) {
    auto transforms = transforms_;
    auto actors = actors_;
    transforms.prepareMatrices(rom_, cameras_.actor(), channelCount);
    actors.prepareModeZeroMatrices(cameras_.registered(), transforms);
    transforms_ = std::move(transforms);
    actors_ = std::move(actors);
}
SceneDrawQueue ScenePlayback::drawQueue(const QVector<QByteArray> &objects,
                                        LordlySceneDrawLayout layout) const {
    return SceneDrawQueue::build(actors_.actorRecords(), objects,
                                 schedule_.mode ? cameras_.actor() : cameras_.registered(), layout);
}
void ScenePlayback::prepareModeTwoDrawing(const LordlyActorLighting &lighting) {
    actors_.prepareModeTwoMatrices(cameras_.actor(), lighting);
}
} // namespace lordly
