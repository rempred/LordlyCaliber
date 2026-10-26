#pragma once
#include "engine/color_combiner.h"
#include <QImage>
#include <QSize>

namespace lordly {
// Bounded desktop consumer of the F3DEX2 drawing lists emitted by battle effects.
// Native vertices, matrices, textures and combiner commands are retained. Desktop
// sampling, coverage, depth and source-over blending approximate the RSP/RDP.
class EffectRaster {
  public:
    struct Result {
        QImage image;
        int triangles = 0, commands = 0;
    };
    static Result draw(const QByteArray &memory, quint32 begin, quint32 end,
                       QSize viewport = {640, 480}, double zoom = 1.5);
};
} // namespace lordly
