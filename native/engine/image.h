#pragma once
#include "engine/rom.h"
namespace lordly {
struct RasterImage {
    int width = 0, height = 0;
    QByteArray rgba;
};
// Decodes a bounded native "64" image without any GUI dependency.
// Source bytes, including row padding and trailing data, are never rewritten.
RasterImage decodeN64Image(const QByteArray &source);
} // namespace lordly
