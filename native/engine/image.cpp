#include "engine/image.h"

namespace lordly {
RasterImage decodeN64Image(const QByteArray &source) {
    if (source.size() < 8 || source.first(2) != "64")
        throw Error("Image does not have a complete native 64 header");
    const int type = u8(source, 2), subtype = u8(source, 3);
    const int width = u16(source, 4), height = u16(source, 6);
    if (!width || !height || width > 8192 || height > 8192 || quint64(width) * height > 4194304)
        throw Error("Image dimensions exceed the native preview budget");
    int stride = 0, paletteCount = 0;
    if (type == 0 && subtype == 2)
        stride = ((width + 3) & ~3) * 2;
    else if (type == 0 && subtype == 3)
        stride = ((width + 3) & ~3) * 4;
    else if ((type == 2 || type == 4) && subtype == 0)
        stride = ((width + 15) & ~15) / 2;
    else if ((type == 2 || type == 4) && subtype == 1)
        stride = (width + 7) & ~7;
    else
        throw Error(QString("Unsupported native image type %1/%2").arg(type).arg(subtype));
    if (type == 2)
        paletteCount = subtype == 0 ? 16 : 256;
    const qsizetype end = 8 + qsizetype(stride) * height;
    if (end + paletteCount * 2 > source.size())
        throw Error("Native image pixels or trailing palette are truncated");
    RasterImage result{width, height, QByteArray(qsizetype(width) * height * 4, 0)};
    auto rgba5551 = [&](qsizetype at, quint16 word) {
        put8(result.rgba, at, quint8((((word >> 11) & 31) * 255 + 15) / 31));
        put8(result.rgba, at + 1, quint8((((word >> 6) & 31) * 255 + 15) / 31));
        put8(result.rgba, at + 2, quint8((((word >> 1) & 31) * 255 + 15) / 31));
        put8(result.rgba, at + 3, word & 1 ? 255 : 0);
    };
    for (int y = 0; y < height; ++y) {
        const qsizetype row = 8 + qsizetype(y) * stride;
        for (int x = 0; x < width; ++x) {
            const qsizetype at = (qsizetype(y) * width + x) * 4;
            if (type == 0) {
                if (subtype == 2)
                    rgba5551(at, u16(source, row + x * 2));
                else
                    for (int c = 0; c < 4; ++c)
                        put8(result.rgba, at + c, u8(source, row + x * 4 + c));
            } else {
                const int packed = u8(source, row + (subtype ? x : x / 2));
                const int value = subtype ? packed : (x & 1) ? packed & 15 : packed >> 4;
                if (type == 2)
                    rgba5551(at, u16(source, end + value * 2));
                else {
                    const quint8 intensity = quint8(subtype ? value : value * 17);
                    for (int c = 0; c < 3; ++c)
                        put8(result.rgba, at + c, intensity);
                    put8(result.rgba, at + 3, 255);
                }
            }
        }
    }
    return result;
}
} // namespace lordly
