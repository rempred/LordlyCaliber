#include "engine/scene_display.h"
#include <limits>
namespace lordly {
bool SceneDisplay::blackout() {
    return lordly_display_black(&state_) != 0;
}
void SceneDisplay::requestReveal() {
    lordly_display_reveal(&state_);
}
void SceneDisplay::submit() {
    if (state_.outstanding == std::numeric_limits<quint32>::max())
        throw Error("Display task count overflow");
    ++state_.outstanding;
}
void SceneDisplay::complete(quint16 event) {
    if (event != 4 && event != 8 && event != 16)
        throw Error("Unsupported display completion event");
    if (!state_.outstanding)
        throw Error("Display completion has no submitted task");
    lordly_display_complete(&state_, event);
}
QImage SceneDisplay::present(const QImage &frame) const {
    if (!black() || frame.isNull())
        return frame;
    QImage hidden(frame.size(), QImage::Format_RGBA8888);
    hidden.fill(Qt::black);
    return hidden;
}
} // namespace lordly
