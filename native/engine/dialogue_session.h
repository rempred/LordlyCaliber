#pragma once
#include "engine/dialogue_draw.h"
#include "engine/dialogues.h"
#include "engine/scene_dialogue_runtime.h"
#include <optional>
namespace lordly {
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
        return runtime_.sounds();
    }

  private:
    QByteArray source(const QByteArray &record) const;
    Rom rom_;
    std::optional<Dialogues> dialogues_;
    DialogueTextEnvironment environment_;
    SceneResources resources_{{1, -1, 0xffff}};
    SceneDialogue windows_{0};
    SceneDialogueRuntime runtime_;
    SceneResourceDispatcher dispatcher_{SceneResourcePassState{}};
    quint64 updates_ = 0;
};
} // namespace lordly
