#include "engine/sprite_object.h"
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
SpriteFrame SpriteObject::frame(int requestedChild, const QByteArray &externalPalette) const {
    const int child = requestedChild < 0 || requestedChild >= children_.size() ? 0 : requestedChild;
    const auto lanes = materialize(child);
    if (lanes.color.isEmpty())
        throw Error("Selected sprite child has no color lane");
    const auto &palette = lanes.palette.isEmpty() ? externalPalette : lanes.palette;
    if (colorFormat_ == 1 && palette.size() != 512)
        throw Error("Indexed sprite requires a complete palette");
    RasterImage image{width_, height_, QByteArray(width_ * height_ * 4, 0)};
    for (int y = 0; y < height_; ++y)
        for (int x = 0; x < width_; ++x) {
            const int out = (y * width_ + x) * 4, at = y * colorStride_;
            if (colorFormat_ == 3) {
                for (int c = 0; c < 4; ++c)
                    put8(image.rgba, out + c, u8(lanes.color, at + x * 4 + c));
            } else {
                const auto word = colorFormat_ == 1 ? u16(palette, u8(lanes.color, at + x) * 2)
                                                    : u16(lanes.color, at + x * 2);
                put8(image.rgba, out, quint8((((word >> 11) & 31) * 255 + 15) / 31));
                put8(image.rgba, out + 1, quint8((((word >> 6) & 31) * 255 + 15) / 31));
                put8(image.rgba, out + 2, quint8((((word >> 1) & 31) * 255 + 15) / 31));
                put8(image.rgba, out + 3, word & 1 ? 255 : 0);
            }
            if (!lanes.alpha.isEmpty()) {
                const auto packed = u8(lanes.alpha, y * alphaStride_ + (alphaFormat_ ? x : x / 2));
                put8(image.rgba, out + 3,
                     alphaFormat_ ? packed : quint8((x & 1 ? packed & 15 : packed >> 4) * 17));
            }
        }
    return {std::move(image), requestedChild, child};
}
} // namespace lordly
