#pragma once
#include "engine/dialogue_draw.h"
#include "engine/dialogues.h"
#include "engine/scene_dialogue_runtime.h"
#include <optional>
namespace lordly {
// Shared resource-pass owner for standalone and composed scene playback.
// Pool/window state belongs to the caller so Director commands see the same pool.
class DialoguePlayback {
  public:
    explicit DialoguePlayback(Rom, std::optional<Dialogues> dialogues = {});
    static DialogueTextEnvironment environment(const Rom &);
    QByteArray source(const QByteArray &record) const;
    void advance(SceneResources &, SceneDialogue &, const DialogueTextEnvironment &, DialogueInput);
    RasterImage image(const SceneResources &, DialogueDrawing &) const;
    int state(int slot, const SceneResources &) const;
    int focusedState(const SceneResources &resources) const {
        const auto &queue = dispatcher_.state().queue;
        return queue.count ? state(queue.entries[0], resources) : -1;
    }
    const QVector<DialogueSoundRequest> &sounds() const {
        return runtime_.sounds();
    }

  private:
    Rom rom_;
    std::optional<Dialogues> dialogues_;
    SceneDialogueRuntime runtime_;
    SceneResourceDispatcher dispatcher_{SceneResourcePassState{}};
};
// Standalone resource-pass preview with explicitly supplied substitution context.
// Scene playback supplies its own actor placement, roster and shared resources.
class DialogueSession {
  public:
    DialogueSession(Rom, int archive, quint16 entry, DialogueTextEnvironment,
                    std::optional<Dialogues> dialogues = {});
    void advance(DialogueInput);
    void resume();
    RasterImage image(DialogueDrawing &) const;
    quint64 updates() const {
        return updates_;
    }
    int state() const;
    int activeWindows() const;
    const QVector<DialogueSoundRequest> &sounds() const {
        return playback_.sounds();
    }

  private:
    DialogueTextEnvironment environment_;
    SceneResources resources_{{1, -1, 0xffff}};
    SceneDialogue windows_{0};
    DialoguePlayback playback_;
    quint64 updates_ = 0;
};
} // namespace lordly
