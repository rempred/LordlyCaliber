#pragma once
#include "engine/dialogue_archive.h"
#include "engine/dialogue_draw.h"
#include "engine/scene_dialogue_runtime.h"
namespace lordly {
// Standalone resource-pass preview with explicitly supplied substitution context.
// Scene playback supplies its own actor placement, roster and shared resources.
class DialogueSession {
  public:
    DialogueSession(Rom, int archive, quint16 entry, DialogueTextEnvironment);
    void advance(DialogueInput);
    void resume();
    RasterImage image(DialogueDrawing &) const;
    quint64 updates() const {
        return updates_;
    }
    int state() const;
    int activeWindows() const;
    const QVector<DialogueSoundRequest> &sounds() const {
        return runtime_.sounds();
    }

  private:
    Rom rom_;
    DialogueTextEnvironment environment_;
    SceneResources resources_{{1, -1, 0xffff}};
    SceneDialogue windows_{0};
    SceneDialogueRuntime runtime_;
    SceneResourceDispatcher dispatcher_{SceneResourcePassState{}};
    quint64 updates_ = 0;
};
} // namespace lordly
