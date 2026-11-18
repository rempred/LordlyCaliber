#include "engine/scene_report_art.h"
#include "engine/field_resources.h"
#include "game/director/text_metrics.h"
#include <algorithm>

namespace lordly {
namespace {
QRgb color(quint16 v) {
    auto channel = [](int c) { return (c * 255 + 15) / 31; };
    return qRgba(channel((v >> 11) & 31), channel((v >> 6) & 31), channel((v >> 1) & 31),
                 (v & 1) ? 255 : 0);
}
QImage indexed(const QByteArray &pixels, int at, const QByteArray &palette, int pal, int width,
               int height) {
    const int stride = (width + 1) / 2;
    if (at < 0 || at + stride * height > pixels.size() || pal < 0 || pal >= palette.size())
        throw Error("Truncated report texture");
    QImage result(width, height, QImage::Format_ARGB32);
    for (int y = 0; y < height; ++y) {
        auto *row = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            int byte = u8(pixels, at + y * stride + x / 2),
                index = (x & 1) ? (byte & 15) : (byte >> 4);
            row[x] = color(u16(palette, pal + 2 * index));
        }
    }
    return result;
}
QByteArray unpack(const Rom &rom, quint32 key, int limit) {
    auto bytes = fieldResource(rom, key);
    const auto n = u32(bytes, 0);
    if (!n || n > quint32(limit))
        throw Error("Report artwork exceeds its resource budget");
    return decodeLzss(bytes.mid(4), n);
}
// Each glyph is a continuous 2-bit bitmap with padding only at the end of
// the character. Normal letters occupy 39 bytes and compact letters 16.
QImage fontSheet(const QByteArray &packed, int width, int height, const std::array<QRgb, 4> &ink,
                 int expected) {
    const int bytes = (width * height * 2 + 7) / 8;
    if (expected < 1 || expected > 4096 || packed.size() != expected * bytes)
        throw Error("Invalid report font atlas");
    constexpr int columns = 32;
    QImage result(columns * width, ((expected + columns - 1) / columns) * height,
                  QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    for (int i = 0; i < expected; ++i)
        for (int y = 0; y < height; ++y) {
            auto *row = reinterpret_cast<QRgb *>(result.scanLine((i / columns) * height + y)) +
                        (i % columns) * width;
            for (int x = 0; x < width; ++x) {
                const int bit = (y * width + x) * 2;
                row[x] = ink[(u8(packed, i * bytes + bit / 8) >> (6 - (bit & 7))) & 3];
            }
        }
    return result;
}
// Sample a native texture rectangle with a one-to-one texel step. Clamp
// extends a border's last texel; only the masked axes repeat.
void texture(QPainter &p, QRect dest, const QImage &src, int u = 0, int v = 0, bool wrapX = false,
             bool wrapY = false) {
    const QRect clip = dest.intersected(QRect(0, 0, 320, 240));
    if (clip.isEmpty())
        return;
    QImage image(clip.size(), QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        const int t = v + y + clip.top() - dest.top();
        const auto *in = reinterpret_cast<const QRgb *>(
            src.constScanLine(wrapY ? t % src.height() : std::clamp(t, 0, src.height() - 1)));
        auto *out = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const int s = u + x + clip.left() - dest.left();
            out[x] = in[wrapX ? s % src.width() : std::clamp(s, 0, src.width() - 1)];
        }
    }
    p.drawImage(clip.topLeft(), image);
}
} // namespace
SceneReportArt::SceneReportArt(const Rom &rom) {
    // Resident panel initialization at z64 51CE4 and 51D08 loads these two
    // archives. Several textures also have unused lookalikes elsewhere in ROM.
    const auto pixels = unpack(rom, 0x1dfa632, 0x10000), pal = unpack(rom, 0x1dfb536, 0x1000);
    paper_ = indexed(pixels, 0, pal, 0, 64, 64).flipped(Qt::Horizontal);
    for (int i = 0; i < 11; ++i)
        trim_[i] = indexed(pixels, 0x800 + i * 64, pal, 0x20 + (i / 4) * 32, 16, 8);
    finger_ = indexed(pixels, 0xac0, pal, 0x80, 16, 12).flipped(Qt::Horizontal);
    more_ = indexed(pixels, 0xb20, pal, 0xa0, 64, 16);
    arrows_ = indexed(pixels, 0xd20, pal, 0xc0, 16, 8);
    scroll_ = indexed(pixels, 0xd20, pal, 0x180, 16, 16);
    const auto archive = fieldMembers(fieldResource(rom, fieldChild(rom, 0x2093576, 0)));
    if (archive.size() < 3)
        throw Error("Missing Hugo's Report message frame");
    for (int i = 0; i < 3; ++i) {
        const auto &b = archive[i].data;
        if (u8(b, 0) != 'K' || u8(b, 2) != 2 || u8(b, 3) != 0 || u16(b, 4) != 16 || u16(b, 6) != 61)
            throw Error("Invalid Hugo's Report message frame");
        help_[i] = indexed(b, 8, b, 8 + 8 * 61, 16, 61);
    }
    const auto labels = unpack(rom, fieldChild(rom, 0x18855fe, 8), 0x10000);
    if (labels.first(4) != QByteArray::fromHex("36340200") || u16(labels, 4) != 32 ||
        u16(labels, 6) != 24)
        throw Error("Invalid Hugo's Report label atlas");
    labels_ = indexed(labels, 8, labels, 8 + 16 * 24, 32, 24);
    metrics_.normal = fieldResource(rom, 0x218c450);
    metrics_.alternate = fieldResource(rom, 0x218de48);
    const auto codes = fieldResource(rom, 0x216c9de);
    if (codes.size() % 2 || codes.size() > 8192)
        throw Error("Invalid report double-byte character directory");
    for (int at = 0; at < codes.size(); at += 2)
        wideCodes_.push_back(u16(codes, at));
    if (!std::is_sorted(wideCodes_.cbegin(), wideCodes_.cend()))
        throw Error("Unsorted report double-byte character directory");
    std::array<QRgb, 4> ink;
    for (int i = 0; i < 4; ++i)
        ink[i] = color(u16(rom.bytes(), 0x66b88 + 2 * i));
    normal_ = fontSheet(fieldResource(rom, 0x218c4fa), 11, 14, ink, metrics_.normal.size());
    compact_ = fontSheet(fieldResource(rom, 0x218def2), 8, 8, ink, metrics_.alternate.size());
    wide_ = fontSheet(fieldResource(rom, 0x216e296), 11, 14, ink, wideCodes_.size());
}
void SceneReportArt::frame(QPainter &p, QRect box, int preset, const QByteArray &record,
                           bool front) const {
    if (box.isEmpty())
        return;
    const int x = box.x(), y = box.y(), w = box.width(), h = box.height(), r = x + w, b = y + h;
    if (preset == 5) {
        p.save();
        p.setClipRect(box.adjusted(1, 1, -2, -2), Qt::IntersectClip);
        const int side = std::min(8, w / 2), foot = std::min(8, h);
        for (int i : {1, 0, 2}) {
            const int left = i == 0 ? x : i == 1 ? x + side : r - side;
            const int width = i == 1 ? w - 2 * side : side;
            texture(p, {left, y, width, h - foot}, help_[i], 0, 0, i == 1);
            texture(p, {left, b - foot, width, foot}, help_[i], 0, 61 - foot, i == 1);
        }
        p.restore();
        return;
    }
    // The category draw callback 002978F4 sets flag 8 according to menu focus.
    const bool lit = (preset == 4 || preset == 7) ? front : (u8(record, 0x14) & 8) != 0;
    drawPaper(p, box, record, lit);
    const auto kind = u16(record, 0x16);
    if (kind & 0x400) {
        texture(p, {x - 4, y, 8, h}, trim_[8], 0, 0, false, true);
        texture(p, {r - 4, y, 8, h}, trim_[8], 8, 0, false, true);
        for (int i : {9, 10}) {
            const int top = i == 9 ? b - 4 : y - 4;
            texture(p, {x - 4, top, w + 4, 8}, trim_[i], 8, 0);
            texture(p, {r - 4, top, 8, 8}, trim_[i]);
        }
    } else {
        const int i = (kind & 0x100) ? 4 : 0;
        texture(p, {x - 4, y - 4, w + 4, 8}, trim_[i], 8);
        texture(p, {x - 4, y - 4, 8, h + 4}, trim_[i], 8);
        texture(p, {r - 4, y - 4, 8, h + 4}, trim_[i + 1], 8);
        texture(p, {x - 4, b - 4, w + 4, 8}, trim_[i + 3], 8);
        texture(p, {r - 4, b - 4, 8, 8}, trim_[i + 2], 8);
    }
}
void SceneReportArt::drawPaper(QPainter &p, QRect box, const QByteArray &record, bool lit) const {
    const int u = (64 - box.width() % 64) % 64;
    if (lit) {
        texture(p, box, paper_, u, 63, true, true);
        return;
    }
    // An inactive panel shades its two triangles. Native vertex order is
    // bottom right, top right, top left and bottom left.
    const QRect clip = box.intersected(QRect(0, 0, 320, 240));
    if (clip.isEmpty())
        return;
    std::array<std::array<float, 3>, 4> shade;
    for (int vertex = 0; vertex < 4; ++vertex)
        for (int c = 0; c < 3; ++c)
            shade[vertex][c] = u8(record, 0x9c + 16 * vertex + c) / 255.f;
    QImage image(clip.size(), QImage::Format_ARGB32);
    for (int j = 0; j < image.height(); ++j) {
        auto *out = reinterpret_cast<QRgb *>(image.scanLine(j));
        const int row = j + clip.top() - box.y();
        const auto *paper = reinterpret_cast<const QRgb *>(paper_.constScanLine((row + 63) % 64));
        const float fy = (row + 0.5f) / box.height();
        for (int i = 0; i < image.width(); ++i) {
            const int col = i + clip.left() - box.x();
            const float fx = (col + 0.5f) / box.width();
            const auto texel = paper[(col + u) % 64];
            const float br = std::min(fx, fy), tr = std::max(0.f, fx - fy),
                        tl = 1 - std::max(fx, fy), bl = std::max(0.f, fy - fx);
            auto channel = [&](int c, int value) {
                return qRound(value * (br * shade[0][c] + tr * shade[1][c] + tl * shade[2][c] +
                                       bl * shade[3][c]));
            };
            out[i] = qRgba(channel(0, qRed(texel)), channel(1, qGreen(texel)),
                           channel(2, qBlue(texel)), qAlpha(texel));
        }
    }
    p.drawImage(clip.topLeft(), image);
}
void SceneReportArt::line(QPainter &p, const QByteArray &bytes, QPoint pos) const {
    int x = pos.x(), y = pos.y(), origin = x, firstEnd = x;
    bool compact = false;
    for (int i = 0; i < bytes.size(); ++i) {
        const int c = u8(bytes, i);
        if (c == 14) {
            compact = true;
            origin = x;
            firstEnd = x;
            continue;
        }
        if (c == 15) {
            compact = false;
            x = std::max(x, firstEnd);
            y = pos.y();
            continue;
        }
        if (c == 16 && i + 1 < bytes.size()) {
            const int indent = u8(bytes, ++i);
            firstEnd = x;
            const int offset = indent >= 'a'   ? indent - 'a' + 10
                               : indent >= 'A' ? indent - 'A' + 10
                                               : indent - '0';
            x = origin + offset;
            y = pos.y() + 7;
            continue;
        }
        const QImage *sheet = compact ? &compact_ : &normal_;
        int index = c < 128 ? c + 16 : c, w = compact ? 8 : 11, h = compact ? 8 : 14;
        if (lordly_text_lead_byte(c)) {
            if (++i >= bytes.size())
                throw Error("Truncated report character");
            const int code = (c << 8) | u8(bytes, i);
            const auto found = std::lower_bound(wideCodes_.cbegin(), wideCodes_.cend(), code);
            index = found != wideCodes_.cend() && *found == code ? int(found - wideCodes_.cbegin())
                                                                 : -1;
            sheet = &wide_;
            w = 11;
            h = 14;
        } else if (c < 17)
            continue;
        const int advance = metrics_.glyphWidth(c, compact);
        if (c != 32 && index >= 0)
            p.drawImage(QRect(x, y, w, h), *sheet, QRect(index % 32 * w, index / 32 * h, w, h));
        x += advance;
    }
}
void SceneReportArt::decorations(QPainter &p, QRect box, int preset, const QByteArray &r,
                                 const QByteArray &content, bool list, int pageFrame) const {
    const int x = box.x(), y = box.y(), right = x + box.width(), bottom = y + box.height();
    const int scroll = u8(content, 0x5f1), visible = u8(content, 0x5f4), total = u8(content, 0x5f5);
    if (preset == 4 || preset == 7 || list) {
        int selected = list ? int(u8(content, 0x5fc)) - scroll : qint8(u8(r, 0x22));
        if (selected >= 0 && selected < visible)
            p.drawImage(x + 3, y + selected * u8(content, 0x5f6) + (list ? 6 : 8), finger_);
    }
    const int badge = (preset == 4 || preset == 7) ? 0 : preset == 13 ? 1 : preset == 11 ? 2 : -1;
    if (badge >= 0)
        p.drawImage(QRect(x + 6, y - (preset == 13 ? 5 : 3), 32, 8), labels_,
                    QRect(0, badge * 8, 32, 8));
    if (preset == 12 && u8(content, 0x5f0) == 0 && u8(content, 0x5f2) + visible < total)
        p.drawImage(QRect(right - 20, bottom - 20, 15, 15), more_,
                    QRect(pageFrame * 16, 0, 15, 15));
    if (!list || visible < 1 || visible >= total - 1)
        return;
    if (scroll + visible < total)
        p.drawImage(QRect(right - 4, bottom - 12, 7, 7), arrows_, QRect(8, 0, 7, 7));
    if (scroll > 0)
        p.drawImage(QRect(right - 4, y + 4, 7, 7), arrows_, QRect(0, 0, 7, 7));
    // Resident scrollbar 000535D0 rounds the top and length independently.
    const int base = y + 14, span = bottom - 14 - base;
    const auto offset = [&](int rows) { return qRound(float(rows) / float(total - 1) * span); };
    const int first = base + offset(scroll), last = first + offset(visible - 1);
    texture(p, {right - 5, first, 7, last - first - 1}, scroll_, 0, 8);
    texture(p, {right - 5, last - 2, 7, 2}, scroll_, 8, 8);
}
} // namespace lordly
