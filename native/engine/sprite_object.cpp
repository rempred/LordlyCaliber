#include "engine/sprite_object.h"
#include "engine/artwork_dither.h"
#include "game/images/sepia.h"
#include "game/images/stride.h"
#include "game/images/sprite_readers.h"
namespace lordly {
namespace {
int stride(int format, int width) {
    if (format < 0 || format > 3)
        throw Error("Unknown sprite lane format");
    return int(func_00201E38(format, unsigned(width)));
}
QByteArray combine(const QByteArray &base, const QByteArray &delta, int format) {
    if (base.size() != delta.size() || base.isEmpty())
        throw Error("Sprite delta lacks a matching base lane");
    auto result = delta;
    if (lordly_sprite_delta(reinterpret_cast<uint8_t *>(result.data()),
            reinterpret_cast<const uint8_t *>(base.constData()),
            reinterpret_cast<const uint8_t *>(delta.constData()), size_t(delta.size()), unsigned(format)))
        throw Error("Invalid sprite delta format or lane size");
    return result;
}
} // namespace
SpriteFrame SpriteFrame::oriented(int width, int height, bool flipX, bool flipY) const {
    if (width < 1 || height < 1 || width > image.width || height > image.height ||
        qint64(image.width) * image.height * 4 != image.rgba.size() ||
        colorAlpha.size() != qint64(image.width) * image.height)
        throw Error("Actor frame dimensions exceed stored sprite pixels");
    if (width != image.width || height != image.height)
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x)
                if ((x >= width || y >= height) && u8(image.rgba, (y * image.width + x) * 4 + 3))
                    throw Error("Actor frame metadata would crop visible sprite pixels");
    auto result = *this;
    result.image = {width, height, QByteArray(width * height * 4, 0)};
    result.colorAlpha = QByteArray(width * height, 0);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const int sx = flipX ? width - 1 - x : x, sy = flipY ? height - 1 - y : y;
            put8(result.colorAlpha, y * width + x, u8(colorAlpha, sy * image.width + sx));
            for (int c = 0; c < 4; ++c)
                put8(result.image.rgba, (y * width + x) * 4 + c,
                     u8(image.rgba, (sy * image.width + sx) * 4 + c));
        }
    return result;
}
SpriteObject SpriteObject::fromImage(const RasterImage &image, bool dither) {
    if (image.width < 1 || image.height < 1 || image.width > 8192 || image.height > 8192 ||
        qint64(image.width) * image.height > 4194304 ||
        qint64(image.width) * image.height * 4 != image.rgba.size())
        throw Error("New sprite dimensions exceed the pixel budget");
    const int colorStride = stride(2, image.width), alphaStride = stride(1, image.width);
    QByteArray bytes(16 + (colorStride + alphaStride) * image.height, 0);
    put16(bytes, 0, 0x5554);
    put8(bytes, 2, 1);
    put16(bytes, 4, quint16(image.width));
    put16(bytes, 6, quint16(image.height));
    put16(bytes, 8, 0x5554);
    put8(bytes, 11, 3);
    put16(bytes, 12, quint16(image.width));
    put16(bytes, 14, quint16(image.height));
    const int alpha = 16 + colorStride * image.height;
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
            const int pixel = (y * image.width + x) * 4;
            const auto channel = [&](int c) {
                return artworkChannel5(u8(image.rgba, pixel + c), x, y, dither);
            };
            put16(bytes, 16 + y * colorStride + x * 2,
                  quint16((channel(0) << 11) | (channel(1) << 6) | (channel(2) << 1) | 1));
            put8(bytes, alpha + y * alphaStride + x, u8(image.rgba, pixel + 3));
        }
    return decode(std::move(bytes));
}
SpriteObject SpriteObject::decode(QByteArray bytes) {
    LordlySpriteLayout layout;
    if (lordly_sprite_layout(reinterpret_cast<const uint8_t *>(bytes.constData()),
                             size_t(bytes.size()), &layout))
        throw Error("Invalid sprite header, dimensions or child lane bounds");
    SpriteObject result;
    result.width_ = int(layout.width);
    result.height_ = int(layout.height);
    result.colorFormat_ = int(layout.color_format);
    result.alphaFormat_ = int(layout.alpha_format);
    result.colorStride_ = int(layout.color_stride);
    result.alphaStride_ = int(layout.alpha_stride);
    const auto offset = [](size_t at) { return at == SIZE_MAX ? qsizetype(-1) : qsizetype(at); };
    for (unsigned i = 0; i < layout.count; ++i) {
        const auto &child = layout.children[i];
        result.children_ << SpriteChild{child.discriminator, child.flags, child.width, child.height,
            offset(child.color), offset(child.alpha), offset(child.palette)};
    }
    result.source_ = std::move(bytes);
    return result;
}
SpriteLanes SpriteObject::materialize(int child, QVector<bool> &visiting,
                                      QVector<std::optional<SpriteLanes>> &cache,
                                      qsizetype &retained) const {
    if (child < 0 || child >= children_.size())
        throw Error("Sprite child is out of range");
    if (cache[child])
        return *cache[child];
    if (visiting[child])
        throw Error("Sprite child discriminators form a cycle");
    visiting[child] = true;
    const auto &row = children_[child];
    auto lane = [&](qsizetype at, int size) {
        return at < 0 ? QByteArray() : source_.mid(at, size);
    };
    SpriteLanes result{lane(row.colorOffset, colorStride_ * height_),
                       lane(row.alphaOffset, alphaStride_ * height_), lane(row.paletteOffset, 512)};
    retained += result.color.size() + result.alpha.size() + result.palette.size();
    if (retained > 64 * 1024 * 1024)
        throw Error("Sprite materialization exceeds the memory budget");
    if (row.discriminator != child) {
        const auto base = materialize(row.discriminator, visiting, cache, retained);
        if (!(row.flags & 8)) {
            if (!result.color.isEmpty())
                result.color = combine(base.color, result.color, colorFormat_);
            if (!result.alpha.isEmpty())
                result.alpha = combine(base.alpha, result.alpha, alphaFormat_);
            if (!result.palette.isEmpty())
                result.palette = combine(base.palette, result.palette, 2);
        }
    }
    visiting[child] = false;
    cache[child] = result;
    return result;
}
SpriteLanes SpriteObject::materialize(int child) const {
    QVector<bool> visiting(children_.size(), false);
    QVector<std::optional<SpriteLanes>> cache(children_.size());
    qsizetype retained = 0;
    return materialize(child, visiting, cache, retained);
}
SpritePixels SpriteObject::pixels(int child) const {
    const auto lanes = materialize(child);
    if (lanes.color.isEmpty())
        throw Error("Selected sprite child has no color lane");
    const int bytes = colorFormat_ == 1 ? 1 : colorFormat_ == 2 ? 2 : 4;
    SpritePixels result;
    result.color.reserve(width_ * height_ * bytes);
    if (!lanes.alpha.isEmpty())
        result.alpha.reserve(width_ * height_);
    for (int y = 0; y < height_; ++y) {
        result.color += lanes.color.mid(y * colorStride_, width_ * bytes);
        if (!lanes.alpha.isEmpty())
            for (int x = 0; x < width_; ++x) {
                const auto value = u8(lanes.alpha, y * alphaStride_ + (alphaFormat_ ? x : x / 2));
                result.alpha += char(alphaFormat_ ? value : (x & 1 ? value & 15 : value >> 4));
            }
    }
    return result;
}
QByteArray SpriteObject::rebuild(const QMap<int, SpritePixels> &edits) const {
    if (edits.isEmpty())
        return source_;
    QVector<bool> visiting(children_.size(), false);
    QVector<std::optional<SpriteLanes>> cache(children_.size());
    qsizetype retained = 0;
    QVector<SpriteLanes> targets;
    for (int child = 0; child < children_.size(); ++child)
        targets << materialize(child, visiting, cache, retained);
    const int bytes = colorFormat_ == 1 ? 1 : colorFormat_ == 2 ? 2 : 4;
    for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
        if (it.key() < 0 || it.key() >= children_.size())
            throw Error("Edited sprite child is outside its container");
        auto &target = targets[it.key()];
        if (target.color.isEmpty() || it->color.size() != width_ * height_ * bytes ||
            it->alpha.size() != (target.alpha.isEmpty() ? 0 : width_ * height_))
            throw Error("Edited sprite pixels do not match their native lanes");
        for (int y = 0; y < height_; ++y) {
            target.color.replace(y * colorStride_, width_ * bytes,
                                 it->color.mid(y * width_ * bytes, width_ * bytes));
            if (!target.alpha.isEmpty())
                for (int x = 0; x < width_; ++x) {
                    const auto value = u8(it->alpha, y * width_ + x);
                    const int at = y * alphaStride_ + (alphaFormat_ ? x : x / 2);
                    if (!alphaFormat_ && value > 15)
                        throw Error("Sprite I4 alpha exceeds 15");
                    put8(target.alpha, at,
                         alphaFormat_
                             ? value
                             : quint8(x & 1 ? (u8(target.alpha, at) & 0xf0) | value
                                            : (u8(target.alpha, at) & 0x0f) | (value << 4)));
                }
        }
    }
    auto output = source_;
    for (int i = 0; i < children_.size(); ++i) {
        const auto &child = children_[i];
        const auto &target = targets[i], &base = targets[child.discriminator];
        auto write = [&](qsizetype at, const QByteArray &lane, const QByteArray &baseLane,
                         int format) {
            if (at < 0)
                return;
            const auto encoded = child.discriminator == i || (child.flags & 8)
                                     ? lane
                                     : combine(baseLane, lane, format);
            output.replace(at, encoded.size(), encoded);
        };
        write(child.colorOffset, target.color, base.color, colorFormat_);
        write(child.alphaOffset, target.alpha, base.alpha, alphaFormat_);
        write(child.paletteOffset, target.palette, base.palette, 2);
    }
    const auto reopened = decode(output);
    for (int i = 0; i < children_.size(); ++i) {
        const auto actual = reopened.materialize(i);
        if (actual.color != targets[i].color || actual.alpha != targets[i].alpha ||
            actual.palette != targets[i].palette)
            throw Error("Sprite delta rebuild changed a materialized child");
    }
    return output;
}
SpriteFrame SpriteObject::frame(int requestedChild, const QByteArray &externalPalette, int sepia,
                                bool warmTone) const {
    if (sepia < 0 || sepia > 64)
        throw Error("Sprite image-processing count exceeds its bounded domain");
    const int child = requestedChild < 0 || requestedChild >= children_.size() ? 0 : requestedChild;
    const auto lanes = materialize(child);
    if (lanes.color.isEmpty())
        throw Error("Selected sprite child has no color lane");
    const auto &palette = lanes.palette.isEmpty() ? externalPalette : lanes.palette;
    if (colorFormat_ == 1 && (palette.isEmpty() || palette.size() % 2))
        throw Error("Indexed sprite requires a complete palette");
    RasterImage image{width_, height_, QByteArray(width_ * height_ * 4, 0)};
    QByteArray colorAlpha(width_ * height_, 0);
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x) {
            const int out = (y * width_ + x) * 4, at = y * colorStride_;
            if (colorFormat_ == 3) {
                quint32 color = u32(lanes.color, at + x * 4);
                for (int n = 0; n < sepia; ++n)
                    color = lordly_sepia_rgba32(color);
                if (warmTone)
                    color = lordly_warm_rgba32(color);
                for (int c = 0; c < 4; ++c)
                    put8(image.rgba, out + c, quint8(color >> (24 - c * 8)));
            } else {
                auto word = colorFormat_ == 1 ? u16(palette, u8(lanes.color, at + x) * 2)
                                              : u16(lanes.color, at + x * 2);
                for (int n = 0; n < sepia; ++n)
                    word = lordly_sepia_rgba16(word);
                if (warmTone)
                    word = lordly_warm_rgba16(word);
                put8(image.rgba, out, quint8((((word >> 11) & 31) * 255 + 15) / 31));
                put8(image.rgba, out + 1, quint8((((word >> 6) & 31) * 255 + 15) / 31));
                put8(image.rgba, out + 2, quint8((((word >> 1) & 31) * 255 + 15) / 31));
                put8(image.rgba, out + 3, word & 1 ? 255 : 0);
            }
            put8(colorAlpha, y * width_ + x, u8(image.rgba, out + 3));
            if (!lanes.alpha.isEmpty()) {
                const auto packed = u8(lanes.alpha, y * alphaStride_ + (alphaFormat_ ? x : x / 2));
                put8(image.rgba, out + 3,
                     alphaFormat_ ? packed : quint8((x & 1 ? packed & 15 : packed >> 4) * 17));
            }
        }
    return {std::move(image),      requestedChild,         child,
            std::move(colorAlpha), !lanes.alpha.isEmpty(), colorFormat_};
}
} // namespace lordly
