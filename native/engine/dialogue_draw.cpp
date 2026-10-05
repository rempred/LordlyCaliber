#include "engine/dialogue_draw.h"
#include "engine/avatar_artwork.h"
#include "engine/archive.h"
#include "game/director/dialogue_draw_plan.h"
#include <algorithm>
#include <cmath>
#include <optional>
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
        const auto method = u8(bytes, at);
        if (method != 0x85 && method != 0x80)
            throw Error("Unsupported dialogue image archive member");
        const auto size = little(at + 1), decoded = little(at + 5);
        if (!size || decoded > 65536 || quint64(at) + 9 + size > quint64(bytes.size()))
            throw Error("Dialogue image archive exceeds its envelope");
        if (i == index) {
            if (method == 0x80 && size != decoded)
                throw Error("Uncompressed dialogue member has inconsistent sizes");
            auto result = method == 0x80 ? bytes.mid(at + 9, size)
                                         : decodeLh5(bytes.mid(at + 9, size), decoded);
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
    if (portraits_) {
        const auto words = portraits_(identity & 255, variant);
        DialogueTexture result{40, 48, 40, 8, QByteArray(1920, 0), QVector<quint16>(80, 0)};
        if (words.isEmpty())
            return result;
        const auto packed = AvatarArtwork::pack(words);
        result.indices = packed.first(1920);
        for (int i = 0; i < 80; ++i)
            result.palette[i] = u16(packed, 1920 + i * 2);
        return result;
    }
    if (!classes_)
        classes_ = ClassData::load(rom_);
    if (!avatars_)
        avatars_ = AvatarLibrary::load(rom_);
    const int id = identity & 255;
    if (id >= classes_->entries().size())
        throw Error("Dialogue portrait class is outside the current class table");
    const auto token = u16(classes_->entries()[id].appearance, variant ? 2 : 0);
    DialogueTexture texture{40, 48, 40, 8, QByteArray(1920, 0), QVector<quint16>(80, 0)};
    if (!token)
        return texture;
    const auto &bundle = avatars_->bundle(token >> 3);
    const auto palette = avatars_->palette(token);
    texture.indices = bundle.indices;
    for (int i = 0; i < 80; ++i)
        texture.palette[i] = u16(palette, i * 2);
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
    auto half = [&](int at) { return u16(payload, at); };
    auto signedHalf = [&](int at) { return qint16(half(at)); };
    const int left = qint16(u16(window, 6)), top = qint16(u16(window, 8)),
              right = qint16(u16(window, 10)), bottom = qint16(u16(window, 12)),
              width = signedHalf(0x20), height = signedHalf(0x22);
    if (width <= 0 || height <= 0)
        throw Error("Dialogue drawing has invalid original dimensions");
    const float fx = float(right - left + 1) / width, fy = float(bottom - top + 1) / height;
    std::array<int, 4> clip{std::max(0, left), std::max(0, top), std::min(319, right + 1),
                            std::min(239, bottom + 1)};
    QVector<DialogueRectangle> result;
    quint8 opacity = 255;
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
        result << DialogueRectangle{std::move(texture), bounds, scissor, uv, {dx, dy}, opacity};
    };
    auto drawText = [&](int start, int x, int y, int color, int verticalSpacing) {
        if (verticalSpacing < '0' || verticalSpacing > '9')
            throw Error("Invalid dialogue line spacing");
        const int end = int(payload.indexOf(char(0), start));
        if (start < 0x178 || start >= payload.size() || end < start)
            throw Error("Dialogue text exceeds its terminated output buffer");
        const auto text = payload.mid(start, end - start), metrics = resource(0x218c450);
        const int origin = x;
        int spacing = 0, lineGap = verticalSpacing - '0';
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
    };
    struct Event {
        LordlyDialogueDrawService service;
        QVector<int> args;
    };
    QVector<Event> events;
    auto sink = [](void *context, LordlyDialogueDrawService service, const int32_t *args,
                   int count) {
        auto &out = *static_cast<QVector<Event> *>(context);
        Event event{service, {}};
        for (int i = 0; i < count; ++i)
            event.args << args[i];
        out << event;
    };
    if (lordly_dialogue_draw_plan(reinterpret_cast<const uint8_t *>(window.constData()),
                                  reinterpret_cast<const uint8_t *>(payload.constData()), sink,
                                  &events))
        throw Error("Dialogue drawing references an invalid history or text row");
    std::optional<DialogueTexture> texture;
    int verticalSpacing = '3', color = 0, mode = 8;
    for (const auto &event : events) {
        const auto &a = event.args;
        switch (event.service) {
        case LORDLY_DDRAW_TRANSFORM:
            break; // Transform is the explicit record-derived fx/fy above.
        case LORDLY_DDRAW_RESET:
            mode = 8;
            opacity = 255;
            break;
        case LORDLY_DDRAW_STYLE:
            mode = a[0];
            opacity = 255;
            break;
        case LORDLY_DDRAW_RAW:
            if (quint32(a[0]) == 0xfa000000 && mode == 2)
                opacity = quint8(a[1]);
            break; // Other words configure the native pipeline; frameless primitive is transparent.
        case LORDLY_DDRAW_REPEAT_FRAME:
        case LORDLY_DDRAW_FRAME:
        case LORDLY_DDRAW_POINTER_ATLAS:
        case LORDLY_DDRAW_SLICE:
            if (a[0] != 0x2093576 || a[1] != 0)
                throw Error("Unknown dialogue artwork directory");
            texture = frame(a[2]);
            if (event.service == LORDLY_DDRAW_SLICE) {
                if (a[3] < 0 || a[4] <= 0 || quint64(a[3] + 1) * a[4] > quint64(texture->height))
                    throw Error("Dialogue animated artwork slice is outside its image");
                texture->indices =
                    texture->indices.mid(a[3] * a[4] * texture->stride, a[4] * texture->stride);
                texture->height = a[4];
            }
            break;
        case LORDLY_DDRAW_PORTRAIT:
            texture = portrait(a[0], a[1]);
            break;
        case LORDLY_DDRAW_RECTANGLE:
        case LORDLY_DDRAW_TEXTURE_RECTANGLE:
            if (!texture)
                throw Error("Dialogue rectangle has no current texture");
            if (a.last() != 0)
                throw Error("Unsupported dialogue texture tile");
            if (event.service == LORDLY_DDRAW_RECTANGLE)
                rectangle(*texture, a[0], a[1], a[2], a[3], a[4], a[5], a[4] + a[2] - a[0],
                          a[5] + a[3] - a[1], clip);
            else
                rectangle(*texture, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], clip);
            break;
        case LORDLY_DDRAW_TEXT_RESET:
            break;
        case LORDLY_DDRAW_TEXT_FORMAT:
            verticalSpacing = a[0];
            color = a[1] - '0';
            break;
        case LORDLY_DDRAW_TEXT:
            drawText(a[0], a[1], a[2], color, verticalSpacing);
            break;
        case LORDLY_DDRAW_SCISSOR:
            clip = {std::clamp(a[0], 0, 319), std::clamp(a[1], 0, 239), std::clamp(a[2], 0, 319),
                    std::clamp(a[3], 0, 239)};
            break;
        case LORDLY_DDRAW_FINISH:
            break;
        }
    }
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
                if (!c.opacity)
                    continue;
                for (int channel = 0; channel < 3; ++channel) {
                    const int shift = 11 - channel * 5;
                    const int source = (((p >> shift) & 31) * 255 + 15) / 31;
                    put8(next, at + channel,
                         quint8((source * c.opacity + u8(next, at + channel) * (255 - c.opacity) +
                                 127) /
                                255));
                }
                put8(next, at + 3,
                     quint8(c.opacity + (u8(next, at + 3) * (255 - c.opacity) + 127) / 255));
            }
    }
    image.rgba = std::move(next);
}
} // namespace lordly
