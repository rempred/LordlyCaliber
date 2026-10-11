#pragma once
#include "engine/dialogue_player.h"
#include "engine/scene_dialogue.h"
#include "engine/scene_resource_dispatch.h"
namespace lordly {
// Host ownership for dialogue payloads formerly copied through the native heap.
// Sources must be complete NUL-terminated byte strings from the selected archive.
class SceneDialogueRuntime {
  public:
    using Source = std::function<QByteArray(const QByteArray &window)>;
    struct Window {
        QByteArray source;
        DialogueTextFrame frame;
    };
    bool service(SceneResources &, QByteArray &scratch, SceneResourcePassState &,
                 const SceneResourceDispatcher::Call &, SceneDialogue &,
                 const DialogueTextEnvironment &, DialogueInput, const QByteArray &shake,
                 const Source &);
    const Window *window(int slot, const SceneResources &) const;
    const QVector<DialogueSoundRequest> &sounds() const {
        return sounds_;
    }
    void clearSounds() {
        sounds_.clear();
    }

  private:
    bool apply(SceneResources &, QByteArray &, SceneResourcePassState &,
               const SceneResourceDispatcher::Call &, SceneDialogue &,
               const DialogueTextEnvironment &, DialogueInput, const QByteArray &, const Source &);
    QHash<quint32, Window> saved_;
    std::optional<Window> current_;
    int currentSlot_ = -1;
    quint32 nextHandle_ = 1;
    QVector<DialogueSoundRequest> sounds_;
};
} // namespace lordly
