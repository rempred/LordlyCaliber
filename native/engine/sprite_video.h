#pragma once
#include "engine/sprite_library.h"
#include <functional>
namespace lordly {
enum class SpriteVideoFormat { Mp4, AnimatedPng, Webm };
struct SpriteVideoOptions {
    int scale = 4;
    int repeats = 1; // Animated PNG accepts zero for continuous looping.
    QColor background = Qt::black;
    SpriteVideoFormat format = SpriteVideoFormat::Mp4;
};
struct SpriteVideoPlan {
    QSize size;
    int ticks = 0;
};
SpriteVideoPlan spriteVideoPlan(const SpriteAsset &, const SpriteVideoOptions &);
// Preserve the studio's 30 Hz timing. Return false on cancellation; the
// destination is replaced atomically only after successful finalization.
bool exportSpriteVideo(const SpriteAsset &, const QString &path, const SpriteVideoOptions &,
                       const std::function<bool(int completed, int total)> &progress = {});
bool exportSpriteAnimatedPng(const SpriteAsset &, const QString &, const SpriteVideoOptions &,
                             const std::function<bool(int, int)> &);
bool exportSpriteWebm(const SpriteAsset &, const QString &, const SpriteVideoOptions &,
                      const std::function<bool(int, int)> &);
} // namespace lordly
