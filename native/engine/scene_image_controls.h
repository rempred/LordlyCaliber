#pragma once
#include "engine/scene_sprites.h"
namespace lordly {
// The image owner retains detached direction arrows. Only attached sprites
// participate in the normal pose sweep; draw completion updates visibility.
class SceneImageControls {
  public:
    void enable(quint32 bank, QByteArray &context, SceneSprites &);
    void disable(QByteArray &context, SceneSprites &);
    void finishDraw(const QByteArray &context, SceneSprites &);
    bool enabled() const {
        return enabled_;
    }
    QByteArray retained(int direction) const;

  private:
    bool enabled_ = false;
    std::array<std::optional<SpritePosePlayer>, 4> arrows_;
    std::array<quint64, 4> identities_{};
};
} // namespace lordly
