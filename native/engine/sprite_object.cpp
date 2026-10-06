#include "engine/sprite_object.h"
#include "engine/artwork_dither.h"
#include "game/images/sepia.h"
namespace lordly {
namespace {
int stride(int format, int width) {
    switch (format) {
    case 0:
        return ((width + 15) / 16) * 8;
    case 1:
        return ((width + 7) / 8) * 8;
    case 2:
        return ((width + 3) / 4) * 8;
    case 3:
        return ((width + 3) / 4) * 16;
    }
    throw Error("Unknown sprite lane format");
}
QByteArray combine(const QByteArray &base, const QByteArray &delta, int format) {
    if (base.size() != delta.size() || base.isEmpty())
        throw Error("Sprite delta lacks a matching base lane");
    auto result = delta;
    for (qsizetype i = 0; i < delta.size();) {
        if (format == 0) {
            const int b = u8(base, i), d = u8(delta, i);
            put8(result, i++,
                 quint8(((((b >> 4) - (d >> 4)) & 15) << 4) | (((b & 15) - (d & 15)) & 15)));
        } else if (format == 1) {
            put8(result, i, quint8(u8(base, i) - u8(delta, i)));
            ++i;
        } else if (format == 2) {
            put16(result, i, quint16(u16(base, i) - u16(delta, i)));
            i += 2;
        } else {
            put8(result, i, u8(base, i) ^ u8(delta, i));
            ++i;
        }
    }
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
    if (bytes.size() < 8 || u16(bytes, 0) != 0x5554 || bytes.size() > 64 * 1024 * 1024)
        throw Error("Invalid native sprite container");
    SpriteObject result;
    result.width_ = u16(bytes, 4);
    result.height_ = u16(bytes, 6);
    if (!result.width_ || !result.height_ || result.width_ > 8192 || result.height_ > 8192 ||
        quint64(result.width_) * result.height_ > 4194304)
        throw Error("Sprite dimensions exceed the pixel budget");
    const int flags = u8(bytes, 3), count = u8(bytes, 2);
    if (!count)
        throw Error("Sprite has no child variants");
    result.colorFormat_ = flags & 2 ? 3 : flags & 4 ? 1 : 2;
    result.alphaFormat_ = flags & 1 ? 0 : 1;
    result.colorStride_ = stride(result.colorFormat_, result.width_);
    result.alphaStride_ = stride(result.alphaFormat_, result.width_);
    qsizetype cursor = 8;
    for (int i = 0; i < count; ++i) {
        if (u16(bytes, cursor) != 0x5554)
            throw Error("Sprite child header is missing");
        SpriteChild child{u8(bytes, cursor + 2), u8(bytes, cursor + 3), u16(bytes, cursor + 4),
                          u16(bytes, cursor + 6)};
        cursor += 8;
        if (child.flags & 1) {
            child.colorOffset = cursor;
            cursor += result.colorStride_ * result.height_;
        }
        if (child.flags & 2) {
            child.alphaOffset = cursor;
            cursor += result.alphaStride_ * result.height_;
        }
        if (child.flags & 4) {
            child.paletteOffset = cursor;
            cursor += 512;
        }
        if (cursor > bytes.size())
            throw Error("Sprite child lanes exceed their source");
        if (child.discriminator >= count)
            throw Error("Sprite child discriminator is out of range");
        result.children_ << child;
    }
    if (cursor != bytes.size())
        throw Error("Sprite container has unaccounted trailing bytes");
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
