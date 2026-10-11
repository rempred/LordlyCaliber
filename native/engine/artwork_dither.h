#pragma once
#include <QColor>
#include <algorithm>
#include <cmath>
namespace lordly {
// Browser import's 4x4 ordered pattern, measured in native five-bit RGB steps.
// Apply during color assignment; alpha and palette construction stay unchanged.
inline double orderedColorOffset(int x, int y) {
    constexpr int bayer[]{0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    return (bayer[(y & 3) * 4 + (x & 3)] - 7.5) / 8.;
}
inline int artworkChannel5(int value, int x, int y, bool dither) {
    if (!dither || value == 0 || value == 255)
        return (value * 31 + 127) / 255;
    return std::clamp(int(std::floor(value * 31. / 255 + orderedColorOffset(x, y) + .5)), 0, 31);
}
inline QColor orderedColor8(QColor color, int x, int y) {
    const double offset = orderedColorOffset(x, y) * 255 / 31;
    const auto channel = [&](int value) {
        if (value == 0 || value == 255)
            return value;
        return std::clamp(int(std::floor(value + offset + .5)), 0, 255);
    };
    return {channel(color.red()), channel(color.green()), channel(color.blue()), color.alpha()};
}
} // namespace lordly
