#pragma once
#include "engine/director_session.h"
#include "engine/scene_actors.h"
#include "engine/scene_audio.h"
#include "engine/scene_camera.h"
#include "engine/scene_crop.h"
#include "engine/scene_dialogue.h"
#include "engine/scene_dialogue_context.h"
#include "engine/scene_effects.h"
#include "engine/scene_fade.h"
#include "engine/scene_image_controls.h"
#include "engine/scene_image_motion.h"
#include "engine/scene_image_presentation.h"
#include "engine/scene_iris.h"
#include "engine/scene_panels.h"
#include "engine/scene_projection.h"
#include "engine/scene_ribbons.h"
#include "engine/scene_sprites.h"
#include "engine/scene_transforms.h"
#include "game/director/scene_schedule.h"
namespace lordly {
struct ScenePlaybackStart {
    int mode = 2;
    quint8 presentation = 0, alternate = 0, delayedCounter = 0;
    DirectorLaunchTranslations translations;
    std::optional<quint32> sceneFlags;
    std::optional<LordlyActorLighting> lighting;
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
    // Render preparation is separate from update cadence. This prepares main
    // and shadow matrices; final draw ordering remains separate.
    void prepareModeZeroDrawing(int channelCount);
    void prepareModeTwoDrawing(const LordlyActorLighting &lighting);
    const std::optional<LordlyActorLighting> &lighting() const {
        return lighting_;
    }
    SceneDrawQueue drawQueue(const QVector<QByteArray> &objects, LordlySceneDrawLayout) const;
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
    SceneCrop &crop() {
        return crop_;
    }
    const SceneCrop &crop() const {
        return crop_;
    }
    const ScenePanels &panels() const {
        return panels_;
    }
    ScenePanels &panels() {
        return panels_;
    }
    void supplyAudio(SceneAudio audio) {
        audio_ = std::move(audio);
    }
    std::optional<SceneAudio> &audio() {
        return audio_;
    }
    const std::optional<SceneAudio> &audio() const {
        return audio_;
    }
    const SceneEffects &effects() const {
        return effects_;
    }
    SceneTransforms &transforms() {
        return transforms_;
    }
    void supplyImageView(QByteArray context);
    void loadImagePresentation(int row);
    void setImageInput(LordlyImageInput input, quint16 pressed = 0) {
        imageInput_ = input;
        pressed_ = pressed;
    }
    bool skipArmed() const {
        return imageView_ && u8(*imageView_, 0x840) != 0;
    }
    bool imageControlsEnabled() const {
        return imageControls_.enabled();
    }
    void finishImageDraw() {
        if (imageView_)
            imageControls_.finishDraw(*imageView_, sprites_);
    }
    const std::optional<SceneImagePresentation> &imagePresentation() const {
        return imagePresentation_;
    }
    const std::optional<QByteArray> &imageView() const {
        return imageView_;
    }
    const SceneImageMotion &imageMotion() const {
        return imageMotion_;
    }
    const SceneRibbons &ribbons() const {
        return ribbons_;
    }
    // Background handles are caller-owned and must remain valid across snapshots.
    // Handles1/2 identify original/processed presentation images; zero is empty.
    void startIris(const QVector<quint32> &, const QImage &,
                   const std::array<quint32, 20> &backgrounds, int channelCount);
    void replaceIrisBackgrounds(const std::array<quint32, 20> &, int channelCount);
    const std::optional<SceneIris> &iris() const {
        return iris_;
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
    void syncIris();
    void applyIris();
    DirectorSession director_;
    Rom rom_;
    SceneActors actors_;
    SceneSprites sprites_;
    SceneProjection projection_;
    SceneCameras cameras_;
    SceneFade fade_;
    SceneEffects effects_;
    SceneCrop crop_;
    ScenePanels panels_;
    std::optional<SceneAudio> audio_;
    int screenVariant_ = -1;
    SceneTransforms transforms_;
    SceneImageMotion imageMotion_;
    SceneRibbons ribbons_;
    std::optional<SceneIris> iris_;
    std::optional<QByteArray> imageView_;
    std::optional<SceneImagePresentation> imagePresentation_;
    SceneImageControls imageControls_;
    LordlyImageInput imageInput_{};
    quint16 pressed_ = 0;
    std::optional<LordlyActorLighting> lighting_;
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
