#include "engine/scene_images.h"
#include <QJsonArray>
#include <algorithm>
namespace lordly {
DecodedImage decodeRawImage(const QByteArray &bytes, const QJsonObject &layout) {
    const int w = layout["frameWidth"].toInt(layout["width"].toInt()),
              h = layout["frameHeight"].toInt(layout["height"].toInt()),
              banks = layout["paletteBankCount"].toInt(),
              count = banks ? banks : layout["frameCount"].toInt(1);
    if (w < 1 || h < 1 || w > 8192 || h > 8192 || count < 1 || count > 1024 ||
        quint64(w) * h * count > 4194304)
        throw Error("Raw image dimensions exceed the preview budget");
    const auto format = layout["pixelFormat"].toString();
    const bool direct = format == "rgba5551", indexed = format == "ci4" || format == "ci8",
               nibble = format == "ci4" || format == "i4";
    if (!direct && !indexed && format != "i4" && format != "i8")
        throw Error("Unknown raw pixel format");
    const int tight = direct ? w * 2 : nibble ? (w + 1) / 2 : w;
    const int stride = layout["rowStride"].toInt(tight);
    if (stride < tight || stride > 65536)
        throw Error("Invalid raw row stride");
    const int frameBytes = layout["frameBytes"].toInt(stride * h),
              data = layout["dataOffset"].toInt(), palette = layout["paletteOffset"].toInt(),
              colors = layout["paletteCount"].toInt();
    if (frameBytes < qint64(stride) * h || data < 0 || palette < 0 ||
        (indexed && (colors < 1 || colors > (nibble ? 16 : 256))) || (banks && !indexed))
        throw Error("Invalid raw pixel or palette layout");
    const qint64 sourceFrames = banks ? 1 : count;
    if (qint64(data) + qint64(frameBytes) * (sourceFrames - 1) + qint64(stride) * h >
            bytes.size() ||
        (indexed && qint64(palette) + qint64(colors) * 2 * std::max(1, banks) > bytes.size()))
        throw Error("Truncated raw pixels or palettes");
    DecodedImage result;
    if (layout["nativeRecordHeader"].toBool()) {
        if (data != 16 || u16(bytes, 4) != w || u16(bytes, 6) != h ||
            u32(bytes, 8) != bytes.size() - 16 || u32(bytes, 12))
            throw Error("External B5 record disagrees with its pixel layout");
        result.origin = {qint16(u16(bytes, 0)), qint16(u16(bytes, 2))};
    }
    for (int f = 0; f < count; ++f) {
        RasterImage image{w, h, QByteArray(w * h * 4, 0)};
        const qsizetype start = data + qsizetype(frameBytes) * (banks ? 0 : f);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const qsizetype src = start + y * stride + (direct ? x * 2 : nibble ? x / 2 : x);
                const int dst = (y * w + x) * 4;
                const int packed = direct ? 0 : u8(bytes, src);
                const int index = nibble ? (x & 1 ? packed & 15 : packed >> 4) : packed;
                if (indexed && index >= colors)
                    throw Error("Raw pixel index exceeds its palette");
                if (direct || indexed) {
                    const auto word =
                        direct ? u16(bytes, src)
                               : u16(bytes, palette + (index + (banks ? f * colors : 0)) * 2);
                    put8(image.rgba, dst, quint8((((word >> 11) & 31) * 255 + 15) / 31));
                    put8(image.rgba, dst + 1, quint8((((word >> 6) & 31) * 255 + 15) / 31));
                    put8(image.rgba, dst + 2, quint8((((word >> 1) & 31) * 255 + 15) / 31));
                    const bool green = layout["greenChromaKey"].toBool() && word == 0x07c0;
                    put8(image.rgba, dst + 3, word & 1 && !green ? 255 : 0);
                    if (green)
                        for (int c = 0; c < 3; ++c)
                            put8(image.rgba, dst + c, 0);
                } else {
                    for (int c = 0; c < 3; ++c)
                        put8(image.rgba, dst + c, quint8(nibble ? index * 17 : index));
                    put8(image.rgba, dst + 3, 255);
                }
            }
        result.frames << std::move(image);
    }
    const int columns = std::clamp(layout["frameColumns"].toInt(count), 1, count),
              rows = (count + columns - 1) / columns, width = w * columns, height = h * rows;
    if (width > 8192 || height > 8192 || quint64(width) * height > 4194304)
        throw Error("Raw frame arrangement exceeds the preview budget");
    result.composite = {width, height, QByteArray(width * height * 4, 0)};
    for (int f = 0; f < count; ++f)
        for (int y = 0; y < h; ++y)
            result.composite.rgba.replace(((f / columns * h + y) * width + f % columns * w) * 4,
                                          w * 4, result.frames[f].rgba.mid(y * w * 4, w * 4));
    // A composed transparent canvas has no color where every source is transparent.
    // Individual frames retain their source RGB values.
    for (qsizetype p = 0; p < result.composite.rgba.size(); p += 4)
        if (!u8(result.composite.rgba, p + 3))
            for (int c = 0; c < 3; ++c)
                put8(result.composite.rgba, p + c, 0);
    if (layout["nativeRecordHeader"].toBool())
        result.layers << ImageLayer{result.origin, result.composite};
    for (const auto &v : layout["warnings"].toArray())
        result.limitations << v.toString();
    return result;
}
} // namespace lordly
