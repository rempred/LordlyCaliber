#include "engine/sprite_colors.h"
#include "engine/image_colors.h"
#include <algorithm>
namespace lordly {
SpriteAsset spriteToSrgb(SpriteAsset asset) {
    asset.validate();
    for (auto &frame : asset.frames)
        for (auto &layer : frame.layers)
            layer.image = imageToSrgb(layer.image);
    return asset;
}
std::optional<QVector<QRgb>> spriteUsedColors(const SpriteAsset &asset, SpriteColorScope scope,
                                              int frame, int layer,
                                              const std::function<bool()> &cancel) {
    asset.validate();
    if (scope != SpriteColorScope::Layer && scope != SpriteColorScope::Frame &&
        scope != SpriteColorScope::Sequence)
        throw Error("Invalid used-color scope");
    if (frame < 0 || frame >= asset.frames.size() || layer < 0 ||
        layer >= asset.frames[frame].layers.size())
        throw Error("Invalid used-color selection");
    struct Cancelled {};
    const auto check = [&] {
        if (cancel && cancel())
            throw Cancelled{};
    };
    try {
        check();
        QVector<const QImage *> images;
        quint64 pixels = 0;
        for (int f = 0; f < asset.frames.size(); ++f) {
            if (scope != SpriteColorScope::Sequence && f != frame)
                continue;
            const auto &layers = asset.frames[f].layers;
            for (int l = 0; l < layers.size(); ++l) {
                if (scope == SpriteColorScope::Layer && l != layer)
                    continue;
                images << &layers[l].image;
                pixels += quint64(layers[l].image.width()) * layers[l].image.height();
            }
        }
        QVector<QRgb> result;
        result.reserve(qsizetype(pixels));
        quint64 count = 0;
        for (const auto image : images) {
            for (int y = 0; y < image->height(); ++y) {
                const auto row = image->constScanLine(y);
                for (int x = 0; x < image->width(); ++x) {
                    if (!(count++ & 0x3fff))
                        check();
                    const auto p = row + x * 4;
                    result << qRgba(p[0], p[1], p[2], p[3]);
                }
            }
        }
        quint64 comparisons = 0;
        std::sort(result.begin(), result.end(), [&](QRgb a, QRgb b) {
            if (!(comparisons++ & 0x3fff))
                check();
            return a < b;
        });
        check();
        result.erase(std::unique(result.begin(), result.end()), result.end());
        result.squeeze();
        check();
        return result;
    } catch (const Cancelled &) {
        return std::nullopt;
    }
}
} // namespace lordly
