#include "engine/image_colors.h"
#include "engine/rom.h"
#include <QColorSpace>
namespace lordly {
QImage imageToSrgb(const QImage &source) {
    if (source.isNull())
        throw Error("Cannot convert an empty image");
    const QColorSpace srgb(QColorSpace::SRgb);
    auto result = source.colorSpace().isValid() && source.colorSpace() != srgb
                      ? source.convertedToColorSpace(srgb, QImage::Format_RGBA8888)
                      : source.convertToFormat(QImage::Format_RGBA8888);
    if (result.isNull())
        throw Error(
            "Cannot convert this image's color profile to sRGB. Export it as sRGB and retry.");
    result.setColorSpace(srgb);
    return result;
}
} // namespace lordly
