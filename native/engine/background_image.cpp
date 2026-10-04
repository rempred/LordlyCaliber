#include "engine/background_image.h"
#include <QRect>
#include <algorithm>
#include <cmath>
namespace lordly {
namespace {
void dimensions(int width, int height) {
    if (width < 1 || height < 1 || width > 8192 || height > 8192 ||
        quint64(width) * height > 4194304)
        throw Error("Background image exceeds the preview pixel budget");
}
void color(QByteArray &output, qsizetype at, quint16 word) {
    put8(output, at, quint8((((word >> 11) & 31) * 255 + 15) / 31));
    put8(output, at + 1, quint8((((word >> 6) & 31) * 255 + 15) / 31));
    put8(output, at + 2, quint8((((word >> 1) & 31) * 255 + 15) / 31));
    put8(output, at + 3, word & 1 ? 255 : 0);
}
} // namespace
RasterImage decodeKImage(const QByteArray &source) {
    if (source.size() < 8 || u8(source, 0) != 'K')
        throw Error("Invalid K image header");
    const int width = u16(source, 4), height = u16(source, 6);
    dimensions(width, height);
    const int count = width * height;
    RasterImage result{width, height, QByteArray(count * 4, 0)};
    if (!u8(source, 1) && !u8(source, 2) && u8(source, 3) == 2) {
        if (source.size() != 8 + count * 2)
            throw Error("Invalid K direct-color extent");
        for (int i = 0; i < count; ++i)
            color(result.rgba, i * 4, u16(source, 8 + i * 2));
        return result;
    }
    const int bits = u8(source, 3) ? 8 : 4, colors = u8(source, 1) + 1;
    if (u8(source, 2) != 2 || u8(source, 3) > 1 || (bits == 4 && width % 2 && height > 1))
        throw Error("Unsupported K image packing");
    const int palette = 8 + (bits == 4 ? (count + 1) / 2 : count);
    if (source.size() != palette + colors * 2)
        throw Error("Invalid K palette extent");
    for (int i = 0; i < count; ++i) {
        const auto packed = u8(source, 8 + (bits == 4 ? i / 2 : i));
        const int index = bits == 8 ? packed : (i & 1 ? packed & 15 : packed >> 4);
        if (index >= colors)
            throw Error("K image index exceeds its palette");
        color(result.rgba, i * 4, u16(source, palette + index * 2));
    }
    return result;
}
BackgroundImage decodeBackgroundImage(const QByteArray &source) {
    if (source.size() < 24 || source.first(2) != "B5")
        throw Error("Invalid B5 background header");
    BackgroundImage result;
    result.format = u8(source, 2);
    result.declaredLayers = u8(source, 3);
    if (result.format != 0 && result.format != 1 && result.format != 3 && result.format != 5)
        throw Error("Unsupported B5 background format");
    result.reference = {u16(source, 4), u16(source, 6)};
    const int count = result.format == 5 ? 1 : result.declaredLayers;
    if (!count)
        throw Error("Background contains no layers");
    qsizetype at = 8;
    QRect extent;
    qsizetype retained = 0;
    for (int i = 0; i < count; ++i) {
        ImageLayer layer;
        layer.origin = {qint16(u16(source, at)), qint16(u16(source, at + 2))};
        const int width = u16(source, at + 4), height = u16(source, at + 6);
        dimensions(width, height);
        const quint32 size = u32(source, at + 8);
        if (u32(source, at + 12) || quint64(at) + 16 + size > quint64(source.size()) ||
            (retained += qsizetype(width) * height * 4) > 64 * 1024 * 1024)
            throw Error("Background layer exceeds its extent or memory budget");
        const auto pixels = source.mid(at + 16, size);
        const bool huff = result.format == 3 && size >= 14 && u32(pixels, 0) == 0x4855fe00;
        if (huff) {
            layer.image = decodeHuffImage(pixels, true);
            if (layer.image.width != width || layer.image.height != height)
                throw Error("HUFF geometry differs from its background layer");
            if (!result.limitations.contains("HUFF fixed-point rounding is approximated."))
                result.limitations << "HUFF fixed-point rounding is approximated.";
        } else {
            const int colorStride = ((width + 3) & ~3) * (result.format == 3 ? 4 : 2);
            const int maskStride = result.format == 0 ? ((width + 7) & ~7) : 0;
            if (size != quint32((colorStride + maskStride) * height))
                throw Error("Background layer pixel size is inconsistent");
            QByteArray header = QByteArray::fromHex("3634000200000000");
            put8(header, 3, result.format == 3 ? 3 : 2);
            put16(header, 4, quint16(width));
            put16(header, 6, quint16(height));
            layer.image = decodeN64Image(header + pixels.first(colorStride * height));
            if (maskStride)
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                        put8(layer.image.rgba, (y * width + x) * 4 + 3,
                             u8(pixels, colorStride * height + y * maskStride + x));
        }
        const QRect rect(layer.origin, QSize(width, height));
        extent = i ? extent.united(rect) : rect;
        result.layers << std::move(layer);
        at += 16 + size;
    }
    if (at != source.size())
        throw Error("Background has unaccounted trailing bytes");
    dimensions(extent.width(), extent.height());
    result.origin = extent.topLeft();
    result.composite = {extent.width(), extent.height(),
                        QByteArray(extent.width() * extent.height() * 4, 0)};
    for (const auto &layer : result.layers) {
        const auto offset = layer.origin - result.origin;
        for (int y = 0; y < layer.image.height; ++y)
            for (int x = 0; x < layer.image.width; ++x) {
                const int from = (y * layer.image.width + x) * 4;
                const int to = ((y + offset.y()) * extent.width() + x + offset.x()) * 4;
                const double a = u8(layer.image.rgba, from + 3) / 255.;
                if (!a)
                    continue;
                const double b = u8(result.composite.rgba, to + 3) / 255., alpha = a + b * (1 - a);
                for (int c = 0; c < 3; ++c)
                    put8(result.composite.rgba, to + c,
                         quint8(std::round((u8(layer.image.rgba, from + c) * a +
                                            u8(result.composite.rgba, to + c) * b * (1 - a)) /
                                           alpha)));
                put8(result.composite.rgba, to + 3, quint8(std::round(alpha * 255.)));
            }
    }
    if (result.format == 0)
        result.limitations << "The separate I8 plane is previewed as alpha; native occlusion and "
                              "blending are unresolved.";
    if (result.format == 5 && result.declaredLayers != 1)
        result.limitations << "Additional declared layers belong to external resources.";
    if (count > 1)
        result.limitations << "Container record order is used for preview layering.";
    return result;
}
} // namespace lordly
