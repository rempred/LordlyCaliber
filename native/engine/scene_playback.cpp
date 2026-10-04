#include "engine/scene_playback.h"
namespace lordly {
ScenePlayback::ScenePlayback(const Rom &rom, Directors resources, int selector,
                             ScenePlaybackStart start, SceneCameras cameras,
                             ActorInstance::Height height)
    : director_(std::move(resources), selector, start.mode, std::move(start.translations)),
      actors_(rom, start.mode, start.presentation, std::move(height)),
      cameras_(std::move(cameras)) {
    schedule_.mode = quint8(start.mode);
    schedule_.alternate = start.alternate;
    schedule_.delayed_counter = start.delayedCounter;
    if (start.sceneFlags)
        actors_.supplySceneFlags(*start.sceneFlags);
}
void ScenePlayback::setMode(int mode) {
    if (mode < 0 || mode > 2)
        throw Error("Scene mode is outside 0..2");
    director_.setMode(mode);
    actors_.setMode(mode);
    schedule_.mode = quint8(mode);
}
bool ScenePlayback::command(quint32 opcode, const QVector<quint32> &args, int &value) {
    if (opcode == 0x97) {
        if (!schedule_.alternate)
            schedule_.alternate = 1;
        return true;
    }
    if (opcode == 0x39 && args.size() == 1 && args[0] != 0) {
        schedule_.alternate = 0;
        return true;
    }
    if (actors_.service(opcode, args, value) || projection_.service(schedule_.mode, opcode, args) ||
        cameras_.service(schedule_.mode, opcode, args) || fade_.service(opcode, args, value))
        return true;
    return commands_ && *commands_ && (*commands_)(opcode, args, value);
}
bool ScenePlayback::update(LordlySceneService service) {
    switch (service) {
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
    case LORDLY_SCENE_POSES:
        if (!actorsAdvanced_) {
            // The original sweep rechecks alternate at entry. A preceding
            // service can change it even during a normal scheduler invocation.
            if (schedule_.alternate)
                return true;
            actors_.advancePoses();
            actorsAdvanced_ = true;
        }
        if (!supplemental_ || !*supplemental_ || !(*supplemental_)(service))
            return false;
        actorsAdvanced_ = false;
        return true;
    case LORDLY_SCENE_DIRECTOR:
        directorResult_ =
            director_.dispatch([this](quint32 op, const QVector<quint32> &args,
                                      int &value) { return command(op, args, value); },
                               instructionBudget_);
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
    pending_.reset();
    supplemental_ = &supplemental;
    commands_ = &commands;
    instructionBudget_ = instructionBudget;
    const int status = lordly_scene_schedule(&schedule_, invoke, this, stageBudget);
    supplemental_ = nullptr;
    commands_ = nullptr;
    if (status == LORDLY_SCENE_COMPLETE) {
        ++completed_;
        released_ = releaseRequested_;
        return released_ ? Released : Advanced;
    }
    if (status == LORDLY_SCENE_INVALID)
        throw Error("Scene scheduler continuation is invalid");
    if (status == LORDLY_SCENE_BUDGET ||
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
} // namespace lordly
