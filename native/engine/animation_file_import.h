#pragma once
#include "engine/sprite_library.h"
#include <QStringList>
namespace lordly {
// Returns frames timed in 30 Hz preview steps. ZIP timing is resampled at frame
// boundaries; PNG files are naturally sorted and use the supplied frame rate.
SpriteAsset importAnimationFiles(QStringList paths, int imageFramesPerSecond = 15);
} // namespace lordly
