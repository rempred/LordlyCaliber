#pragma once
#include "engine/rom.h"
#include "game/director/display_state.h"
#include <QImage>
namespace lordly {
// Shared VI state at the explicit graphics-completion boundary. Copyable for
// checkpoints; present() hides output without modifying framebuffer contents.
class SceneDisplay {
  public:
    bool blackout();
    void requestReveal();
    void submit();
    void complete(quint16 event = 4);
    bool black() const {
        return state_.black != 0;
    }
    quint32 request() const {
        return state_.reveal_request;
    }
    quint32 outstanding() const {
        return state_.outstanding;
    }
    QImage present(const QImage &) const;

  private:
    LordlyDisplayState state_{};
};
} // namespace lordly
