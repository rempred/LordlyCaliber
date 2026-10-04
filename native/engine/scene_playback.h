#pragma once
#include "engine/director_session.h"
#include "engine/scene_actors.h"
#include "engine/scene_camera.h"
#include "engine/scene_dialogue.h"
#include "engine/scene_dialogue_context.h"
#include "engine/scene_effects.h"
#include "engine/scene_fade.h"
#include "engine/scene_projection.h"
#include "engine/scene_sprites.h"
#include "engine/scene_transforms.h"
#include "game/director/scene_schedule.h"
namespace lordly {
struct ScenePlaybackStart {
    int mode = 2;
    quint8 presentation = 0, alternate = 0, delayedCounter = 0;
    DirectorLaunchTranslations translations;
    std::optional<quint32> sceneFlags;
};
struct SceneDialogueState {
    SceneResources resources;
    SceneDialogue windows;
    SceneDialogue::Project project;
    std::optional<SceneDialogueContext> context;
    SceneDialogue::Portrait portrait;
};
// One normal/alternate Actor-scene scheduler invocation, not a video frame.
// Rendering and the fade's independent permanent-scheduler slot remain separate.
class ScenePlayback {
  public:
    enum Result { Advanced, Suspended, Budget, Released };
    using Update = std::function<bool(LordlySceneService)>;
    ScenePlayback(const Rom &, Directors, int selector, ScenePlaybackStart, SceneCameras cameras,
                  ActorInstance::Height height = {});
    // Actor and sprite poses share the native sweep, in that order.
    // Missing supplemental callbacks suspend, including empty pools
    // whose absence has not been established by the owner of that domain.
    Result advance(const Update &supplemental = {}, const DirectorSession::Service &commands = {},
                   int instructionBudget = 10000, int stageBudget = 64);
    void advanceFade();
    // Render preparation is separate from update cadence. This prepares ordinary
    // main sprites; shadow construction and final draw ordering remain separate.
    void prepareModeZeroMainDrawing(int channelCount);
    void setMode(int mode);
    const LordlySceneSchedule &schedule() const {
        return schedule_;
    }
    void setAlternate(quint8 value) {
        schedule_.alternate = value;
    }
    void setDelayedCounter(quint8 value) {
        schedule_.delayed_counter = value;
    }
    quint64 completedUpdates() const {
        return completed_;
    }
    bool released() const {
        return released_;
    }
    int directorResult() const {
        return directorResult_;
    }
    const QString &reason() const {
        return reason_;
    }
    std::optional<LordlySceneService> pendingService() const {
        return pending_;
    }
    DirectorSession &director() {
        return director_;
    }
    const DirectorSession &director() const {
        return director_;
    }
    SceneActors &actors() {
        return actors_;
    }
    const SceneActors &actors() const {
        return actors_;
    }
    SceneSprites &sprites() {
        return sprites_;
    }
    const SceneSprites &sprites() const {
        return sprites_;
    }
    SceneProjection &projection() {
        return projection_;
    }
    const SceneProjection &projection() const {
        return projection_;
    }
    SceneCameras &cameras() {
        return cameras_;
    }
    const SceneCameras &cameras() const {
        return cameras_;
    }
    SceneFade &fade() {
        return fade_;
    }
    const SceneFade &fade() const {
        return fade_;
    }
    SceneEffects &effects() {
        return effects_;
    }
    const SceneEffects &effects() const {
        return effects_;
    }
    SceneTransforms &transforms() {
        return transforms_;
    }
    const SceneTransforms &transforms() const {
        return transforms_;
    }
    // The six UI resources are shared beyond one scene. Their current state
    // must be supplied by the UI-resource owner, rather than reset on launch.
    void supplyDialogue(SceneResources resources, SceneDialogue windows,
                        SceneDialogue::Project project = {}) {
        dialogue_ =
            SceneDialogueState{std::move(resources), std::move(windows), std::move(project)};
    }
    std::optional<SceneDialogueState> &dialogue() {
        return dialogue_;
    }
    const std::optional<SceneDialogueState> &dialogue() const {
        return dialogue_;
    }

  private:
    static int invoke(void *, LordlySceneService);
    bool update(LordlySceneService);
    bool command(quint32, const QVector<quint32> &, int &);
    DirectorSession director_;
    Rom rom_;
    SceneActors actors_;
    SceneSprites sprites_;
    SceneProjection projection_;
    SceneCameras cameras_;
    SceneFade fade_;
    SceneEffects effects_;
    SceneTransforms transforms_;
    std::optional<SceneDialogueState> dialogue_;
    LordlySceneSchedule schedule_{};
    bool actorsAdvanced_ = false, releaseRequested_ = false, released_ = false;
    bool poseSweepStarted_ = false;
    bool poseBudgetReached_ = false;
    quint64 completed_ = 0;
    int directorResult_ = LORDLY_DIRECTOR_YIELD, instructionBudget_ = 10000;
    std::optional<LordlySceneService> pending_;
    QString reason_;
    const Update *supplemental_ = nullptr;
    const DirectorSession::Service *commands_ = nullptr;
};
} // namespace lordly
