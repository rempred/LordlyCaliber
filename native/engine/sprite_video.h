#pragma once
#include "engine/sprite_library.h"
#include <functional>
namespace lordly {
struct SpriteVideoOptions {
    int scale = 4;
    int repeats = 1;
    QColor background = Qt::black;
};
struct SpriteVideoPlan {
    QSize size;
    int ticks = 0;
};
SpriteVideoPlan spriteVideoPlan(const SpriteAsset &, const SpriteVideoOptions &);
// Encode one 30 Hz sample per visible tick. Return false on cancellation; the
// destination is replaced atomically only after successful finalization.
bool exportSpriteVideo(const SpriteAsset &, const QString &path, const SpriteVideoOptions &,
                       const std::function<bool(int completed, int total)> &progress = {});
} // namespace lordly
