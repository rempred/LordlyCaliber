#include "engine/dialogue_draw.h"
#include "engine/archive.h"
#include <algorithm>
#include <cmath>
namespace lordly {
DialogueDrawing::DialogueDrawing(Rom rom) : rom_(std::move(rom)) {
    rom_.requireSupported();
    if (rom_.revision())
        throw Error("Dialogue drawing currently requires US revision zero");
}
QByteArray DialogueDrawing::resource(quint32 key, bool compressed) const {
    if (!key || (key & 1) || key >= 0x10000000)
        throw Error("Invalid dialogue artwork resource key");
    const quint32 at = key + 0x594280, size = u32(rom_.bytes(), at);
    if (size > 16 * 1024 * 1024 || quint64(at) + 4 + size > quint64(rom_.bytes().size()))
        throw Error("Dialogue artwork exceeds its ROM envelope");
    const auto bytes = rom_.bytes().mid(at + 4, size);
    if (!compressed)
        return bytes;
    const auto decoded = u32(bytes, 0);
    if (decoded > 16 * 1024 * 1024)
        throw Error("Dialogue artwork exceeds its decode budget");
    qsizetype used = 0;
    const auto result = decodeLzss(bytes.mid(4), decoded, &used);
    if (used != size - 4)
        throw Error("Dialogue artwork has unread compressed bytes");
    return result;
}
QByteArray DialogueDrawing::member(quint32 key, int index) {
    const quint64 identity = (quint64(key) << 32) | quint32(index);
    if (members_.contains(identity))
        return members_[identity];
    const auto bytes = resource(key);
    qsizetype at = 0;
    auto little = [&](qsizetype offset) {
        return quint32(u8(bytes, offset)) | (quint32(u8(bytes, offset + 1)) << 8) |
               (quint32(u8(bytes, offset + 2)) << 16) | (quint32(u8(bytes, offset + 3)) << 24);
    };
    for (int i = 0; i <= index; ++i) {
        if (u8(bytes, at) != 0x85)
            throw Error("Unsupported dialogue image archive member");
        const auto size = little(at + 1), decoded = little(at + 5);
        if (!size || decoded > 65536 || quint64(at) + 9 + size > quint64(bytes.size()))
            throw Error("Dialogue image archive exceeds its envelope");
        if (i == index) {
            auto result = decodeLh5(bytes.mid(at + 9, size), decoded);
            members_.insert(identity, result);
            return result;
        }
        at += 9 + size;
    }
    throw Error("Invalid dialogue image member index");
}
DialogueTexture DialogueDrawing::frame(int index) {
    const auto directory = resource(0x2093576);
    const auto bytes = member(u32(directory, 0), index);
    if (u8(bytes, 0) != 'K' || u8(bytes, 2) != 2 || u8(bytes, 3) > 1)
        throw Error("Unsupported dialogue frame pixel format");
    const int width = u16(bytes, 4), height = u16(bytes, 6), bits = u8(bytes, 3) ? 8 : 4;
    const int stride = ((width + (bits == 8 ? 7 : 15)) / (bits == 8 ? 8 : 16)) * 8;
    const int end = 8 + stride * height;
    if (width < 1 || height < 1 || end > bytes.size() || (bytes.size() - end) % 2)
        throw Error("Invalid dialogue frame pixels");
    DialogueTexture texture{width, height, stride, bits, bytes.mid(8, end - 8), {}};
    for (int at = end; at < bytes.size(); at += 2)
        texture.palette << u16(bytes, at);
    texture.wrapX = index == 1 || index == 3;
    texture.mirrorX = texture.mirrorY = index == 3;
    texture.maskHeight = index == 3 ? 16 : 0;
    return texture;
}
DialogueTexture DialogueDrawing::portrait(int identity, bool variant) const {
    const auto token = u16(rom_.bytes(), 0x64be0 + (identity & 255) * 6 + (variant ? 2 : 0));
    DialogueTexture texture{40, 48, 40, 8, QByteArray(1920, 0), QVector<quint16>(80, 0)};
    if (!token)
        return texture;
    const auto directory = resource(0x1dfb6ea);
    const auto bytes = resource(u32(directory, (token >> 3) * 4), true);
    const int palette = 1920 + (token & 7) * 160;
    if (palette + 160 > bytes.size())
        throw Error("Dialogue portrait palette is unavailable");
    texture.indices = bytes.first(1920);
    for (int i = 0; i < 80; ++i)
        texture.palette[i] = u16(bytes, palette + i * 2);
    return texture;
}
DialogueTexture DialogueDrawing::glyph(int index, int color) const {
    const auto bytes = resource(0x218c4fa);
    if (index < 0 || index >= 166 || index * 39 + 39 > bytes.size() || color < 0 || color > 9)
        throw Error("Unsupported dialogue glyph or palette");
    DialogueTexture texture{16, 14, 8, 4, QByteArray(112, 0), {}};
    for (int y = 0; y < 14; ++y)
        for (int x = 0; x < 11; ++x) {
            const int bit = (y * 11 + x) * 2;
            const int value = (u8(bytes, index * 39 + bit / 8) >> (6 - (bit & 7))) & 3;
            const int at = y * 8 + x / 2;
            put8(texture.indices, at, u8(texture.indices, at) | (value << (x & 1 ? 0 : 4)));
        }
    for (int i = 0; i < 4; ++i)
        texture.palette << u16(rom_.bytes(), 0x66b88 + color * 8 + i * 2);
    return texture;
}
QVector<DialogueRectangle> DialogueDrawing::compose(const QByteArray &window,
                                                    const QByteArray &payload) {
    if (window.size() != 168 || payload.size() != 1144)
        throw Error("Dialogue drawing requires complete window and payload records");
    auto byte = [&](int at) { return u8(payload, at); };
    auto half = [&](int at) { return u16(payload, at); };
    auto signedHalf = [&](int at) { return qint16(half(at)); };
    if (byte(0x47) || byte(0x3d) > 1 || byte(0x50) || byte(0x42) > 1 || byte(0x56) != 255 ||
        byte(0x49) != 255)
        throw Error("Unsupported ordinary dialogue presentation state");
    const bool frameless = u8(window, 0x91) & 4;
    const int left = qint16(u16(window, 6)), top = qint16(u16(window, 8)),
              right = qint16(u16(window, 10)), bottom = qint16(u16(window, 12)),
              width = signedHalf(0x20), height = signedHalf(0x22);
    if (width <= 0 || height <= 0)
        throw Error("Dialogue drawing has invalid original dimensions");
    const float fx = float(right - left + 1) / width, fy = float(bottom - top + 1) / height;
    const std::array<int, 4> clip{std::max(0, left), std::max(0, top), std::min(319, right + 1),
                                  std::min(239, bottom + 1)};
    QVector<DialogueRectangle> result;
    auto rectangle = [&](DialogueTexture texture, int x, int y, int x2, int y2, int s0, int t0,
                         int s1, int t1, std::array<int, 4> scissor) {
        std::array<int, 4> bounds{
            int(std::trunc(float(x) * fx)) + left, int(std::trunc(float(y) * fy)) + top,
            int(std::trunc(float(x2 + 1) * fx)) + left, int(std::trunc(float(y2 + 1) * fy)) + top};
        if (bounds[2] < 0 || bounds[3] < 0 || bounds[0] >= 320 || bounds[1] >= 240)
            return;
        const int w = bounds[2] - bounds[0], h = bounds[3] - bounds[1];
        // Opening frames can collapse a rectangle to zero pixels. The native
        // step denominator still uses extent-1 (only extent==1 has a special case).
        const double dx =
                         qint16(int(std::trunc(double(s1 - s0 + 1) * 1024 / (w == 1 ? 1 : w - 1))) -
                                1) /
                         1024.,
                     dy =
                         qint16(int(std::trunc(double(t1 - t0 + 1) * 1024 / (h == 1 ? 1 : h - 1))) -
                                1) /
                         1024.;
        std::array<double, 2> uv{double(s0), double(t0)};
        if (bounds[0] < 0)
            uv[0] += std::trunc(-bounds[0] * dx * 32) / 32;
        if (bounds[1] < 0)
            uv[1] += std::trunc(-bounds[1] * dy * 32) / 32;
        bounds = {std::max(0, bounds[0]), std::max(0, bounds[1]), std::min(320, bounds[2]),
                  std::min(240, bounds[3])};
        result << DialogueRectangle{std::move(texture), bounds, scissor, uv, {dx, dy}};
    };
    auto tile = [&](int index, int x, int y, int x2, int y2) {
        rectangle(frame(index), x, y, x2, y2, 0, 0, x2 - x, y2 - y, clip);
    };
    if (!frameless) {
        tile(1, 8, 0, width - 12, 60);
        tile(0, 0, 0, 7, 60);
        tile(2, width - 11, 0, width - 1, 60);
    }
    const bool hasPortrait = signedHalf(0x14) >= 0;
    const int portraitX = byte(0x3d) ? 7 * half(0x1a) + 16 : 8;
    if (hasPortrait) {
        const bool mirror = u8(window, 0x8a) & 32;
        rectangle(portrait(signedHalf(0x14), byte(0x4a)), portraitX, 5, portraitX + 39, 52,
                  mirror ? 39 : 0, 0, mirror ? 0 : 39, 47, clip);
    }
    if (!(u16(window, 0) & 0x1000) && byte(0x178)) {
        const int start = byte(0x4f) ? half(0xb8 + byte(0x54) * 2) : half(0x36), count = half(0x34);
        if (start > count || count > 768)
            throw Error("Dialogue text exceeds its output buffer");
        const auto text = payload.mid(0x178 + start, count - start), metrics = resource(0x218c450);
        const int origin = left + (frameless ? 0 : hasPortrait && !byte(0x3d) ? 56 : 8);
        int x = origin, y = top + (frameless ? 0 : 5), color = byte(0x4b) < 10 ? byte(0x4b) : 0,
            spacing = 0, lineGap = 3;
        auto drawGlyph = [&](int index) {
            if (index < 0 || index >= metrics.size())
                throw Error("Dialogue glyph exceeds its metric table");
            if (x < 320 && y < 240 && x + 11 >= 0 && y + 14 >= 0)
                result << DialogueRectangle{
                    glyph(index, color),
                    {std::max(0, x), std::max(0, y), std::min(320, x + 11), std::min(240, y + 14)},
                    clip,
                    {double(std::max(0, -x)), double(std::max(0, -y))},
                    {1, 1}};
            x += u8(metrics, index) + spacing;
        };
        for (int i = 0; i < text.size() && u8(text, i); ++i) {
            const auto c = u8(text, i);
            if (c == 10) {
                x = origin;
                y += 14 + lineGap;
                continue;
            }
            if (c == 13) {
                x = origin;
                continue;
            }
            if (c == '{') {
                const auto tag = u8(text, ++i);
                int number = 0;
                for (++i; i < text.size() && u8(text, i) >= '0' && u8(text, i) <= '9'; ++i) {
                    if (number > 100000)
                        throw Error("Dialogue draw tag exceeds its numeric budget");
                    number = number * 10 + u8(text, i) - '0';
                }
                if (u8(text, i) != '}')
                    throw Error("Incomplete dialogue draw tag");
                switch (tag) {
                case 'T':
                    drawGlyph(number - 1);
                    break;
                case 'C':
                    if (number > 9)
                        throw Error("Unsupported dialogue text color");
                    color = number;
                    break;
                case 'H':
                    spacing = number;
                    break;
                case 'V':
                    lineGap = number;
                    break;
                case 'X':
                    x += number;
                    break;
                default:
                    throw Error("Unsupported dialogue draw tag");
                }
                continue;
            }
            if (c < 32 || c >= 128)
                throw Error("Unsupported encoded dialogue character");
            if (c == 32)
                x += u8(metrics, 48) + spacing;
            else
                drawGlyph(c + 16);
        }
    }
    if (byte(0x42) == 1 && !byte(0x4f)) {
        auto indicator = frame(4);
        if (indicator.width != 16 || indicator.height != 64)
            throw Error("Unsupported dialogue continuation artwork");
        const int phase = byte(0x43) >> 6;
        indicator.indices =
            indicator.indices.mid(phase * indicator.stride * 16, indicator.stride * 16);
        indicator.height = 16;
        const int x = 7 * half(0x1a) + (frameless                    ? -16
                                        : hasPortrait && !byte(0x3d) ? 40
                                                                     : -8),
                  y = frameless ? 33 : 38;
        rectangle(std::move(indicator), x, y, x + 15, y + 15, 0, 0, 15, 15, clip);
    }
    if (byte(0x3a) > 1 || byte(0x3b) > 1)
        throw Error("Unsupported dialogue pointer tile");
    if (!frameless)
        rectangle(frame(3), signedHalf(0x30), signedHalf(0x32), signedHalf(0x30) + 7,
                  signedHalf(0x32) + 12, byte(0x3a) * 8, byte(0x3b) * 19, byte(0x3a) * 8 + 7,
                  byte(0x3b) * 19 + 12, {0, 0, 319, 239});
    return result;
}
void DialogueDrawing::paint(RasterImage &image, const QVector<DialogueRectangle> &commands) {
    if (image.width != 320 || image.height != 240 || image.rgba.size() != 320 * 240 * 4)
        throw Error("Dialogue compositor requires a 320 by 240 RGBA target");
    auto next = image.rgba;
    auto mirror = [](int value, int size) {
        const int at = ((value % (size * 2)) + size * 2) % (size * 2);
        return at < size ? at : size * 2 - 1 - at;
    };
    for (const auto &c : commands) {
        const auto &t = c.texture;
        for (int y = std::max({0, c.bounds[1], c.scissor[1]});
             y < std::min({240, c.bounds[3], c.scissor[3]}); ++y)
            for (int x = std::max({0, c.bounds[0], c.scissor[0]});
                 x < std::min({320, c.bounds[2], c.scissor[2]}); ++x) {
                const int sourceX = int(std::floor(c.uv[0] + (x - c.bounds[0]) * c.step[0])),
                          sourceY = int(std::floor(c.uv[1] + (y - c.bounds[1]) * c.step[1]));
                const int sx = t.mirrorX ? mirror(sourceX, t.width)
                               : t.wrapX ? ((sourceX % t.width) + t.width) % t.width
                                         : std::clamp(sourceX, 0, t.width - 1),
                          sy = t.mirrorY ? mirror(sourceY, t.maskHeight)
                                         : std::clamp(sourceY, 0, t.height - 1);
                const auto raw = u8(t.indices, sy * t.stride + (t.bits == 8 ? sx : sx / 2));
                const int index = t.bits == 8 ? raw : (raw >> (sx & 1 ? 0 : 4)) & 15;
                if (index >= t.palette.size())
                    throw Error("Dialogue texel exceeds its palette");
                const auto p = t.palette[index];
                if (!(p & 1))
                    continue;
                const int at = (y * 320 + x) * 4;
                put8(next, at, quint8((((p >> 11) & 31) * 255 + 15) / 31));
                put8(next, at + 1, quint8((((p >> 6) & 31) * 255 + 15) / 31));
                put8(next, at + 2, quint8((((p >> 1) & 31) * 255 + 15) / 31));
                put8(next, at + 3, 255);
            }
    }
    image.rgba = std::move(next);
}
} // namespace lordly
