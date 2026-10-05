#include "engine/scene_ribbons.h"
#include "game/director/path_curve.h"
#include "game/director/scene_matrix.h"
#include <QPainter>
#include <bit>
#include <cmath>
#include <limits>
namespace lordly {
namespace {
QByteArray resource(const Rom &rom, quint32 key) {
    const quint64 at = quint64(key) + 0x594280;
    if (!key || key >= 0x10000000 || key % 2 || at + 4 > quint64(rom.bytes().size()))
        throw Error("Ribbon resource key is outside the ROM");
    const auto size = u32(rom.bytes(), at);
    if (!size || size > 4 * 1024 * 1024 || at + 4 + size > quint64(rom.bytes().size()))
        throw Error("Ribbon resource exceeds its envelope");
    return rom.bytes().mid(at + 4, size);
}
quint32 select(const Rom &rom, quint32 key, int index) {
    const auto table = resource(rom, key);
    if (table.size() % 4 || index < 0 || index >= table.size() / 4)
        throw Error("Ribbon path selector is outside its current ROM table");
    return u32(table, index * 4);
}
void endpoint(const SceneRibbon &r, qint32 percent, qint32 milestone) {
    if (milestone != -1) {
        if (milestone < 0 || milestone >= r.points.size())
            throw Error("Ribbon milestone is outside this path");
    } else if (percent < -1 || percent > 100)
        throw Error("Ribbon percentage must be -1 or zero through 100");
}
uint8_t *data(QByteArray &r) {
    return reinterpret_cast<uint8_t *>(r.data());
}
QVector<QPointF> sample(const QVector<QPointF> &points, int count, double scale) {
    QVector<float> x, y, sx(count), sy(count);
    for (const auto &p : points) {
        x << float(p.x());
        y << float(p.y());
    }
    if (!lordly_path_sample(sx.data(), sy.data(), count, int(points.size()), x.constData(),
                            y.constData(), scale))
        throw Error("Ribbon path has degenerate or unsupported curve coordinates");
    QVector<QPointF> result;
    for (int i = 0; i < count; ++i)
        result << QPointF(sx[i], sy[i]);
    return result;
}
} // namespace
SceneRibbons::SceneRibbons(const Rom &rom) : rom_(rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Scene ribbons require US revision zero");
    // ROM2A1EC4's sampler constant; placement qualified by retained overlay RAM.
    timeScale_ = std::bit_cast<double>((quint64(u32(rom.bytes(), 0x2a8b90)) << 32) |
                                       u32(rom.bytes(), 0x2a8b94));
    for (int i = 0; i < 5; ++i)
        margins_[i] = qint16(u16(rom.bytes(), 0x2b8b00 + i * 2));
}
SceneRibbon SceneRibbons::load(const QVector<quint32> &args,
                               const QMap<int, QPointF> &markers) const {
    if (args.size() != 9 || args[0] >= 20)
        throw Error("Ribbon creation requires a slot in zero through 19 and nine operands");
    const int width = qint32(args[5]), palette = qint32(args[6]);
    if (width < 1 || width > 5 || palette < 0 || palette > 3)
        throw Error("Ribbon width must be 1..5 and palette 0..3");
    SceneRibbon r;
    r.key = select(rom_, select(rom_, 0x1a8d7a6, qint32(args[1])), qint32(args[2]));
    const auto packed = resource(rom_, r.key);
    const auto size = u32(packed, 0);
    if (size < 24 || size > 16 * 12 || size % 12)
        throw Error("Ribbon control-point resource has an invalid size");
    qsizetype consumed = 0;
    const auto bytes = decodeLzss(packed.mid(4), size, &consumed);
    if (consumed != packed.size() - 4)
        throw Error("Ribbon path has unrecognized trailing compressed data");
    for (int at = 0; at < bytes.size(); at += 12) {
        const auto marker = qint32(u32(bytes, at));
        if (marker == -1)
            r.points << QPointF(float(qint32(u32(bytes, at + 4))),
                                float(qint32(u32(bytes, at + 8))));
        else if (markers.contains(marker))
            r.points << markers[marker];
        else
            throw Error(QString("Ribbon path needs map marker %1").arg(marker));
    }
    endpoint(r, qint32(args[3]), qint32(args[4]));
    const auto measured = sample(r.points, 400, timeScale_);
    float length = 0;
    for (int i = 1; i < measured.size(); ++i) {
        const float dx = float(measured[i].x()) - float(measured[i - 1].x()),
                    dy = float(measured[i].y()) - float(measured[i - 1].y());
        const float xx = dx * dx, yy = dy * dy;
        length = length + std::sqrt(xx + yy);
    }
    if (!std::isfinite(length) || length < 6 || length > 8192)
        throw Error("Ribbon length exceeds the preview geometry domain");
    const int count = (int(length) / 2) * 2;
    r.samples = sample(r.points, count, timeScale_);
    r.record = QByteArray(0x88, 0);
    auto &rec = r.record;
    put32(rec, 0xc, count * 32);
    put32(rec, 0x10, 0);
    put32(rec, 0x10 + (r.points.size() - 1) * 4, count - 1);
    for (int i = 1; i < r.points.size() - 1; ++i) {
        float best = std::numeric_limits<float>::infinity();
        int nearest = 0;
        for (int j = 0; j < count; ++j) {
            const float dx = float(r.points[i].x()) - float(r.samples[j].x()),
                        dy = float(r.points[i].y()) - float(r.samples[j].y());
            const float xx = dx * dx, yy = dy * dy, distance = xx + yy;
            if (distance < best) {
                best = distance;
                nearest = j;
            }
        }
        put32(rec, 0x10 + i * 4, nearest);
    }
    put32(rec, 0x50, args[3]);
    put32(rec, 0x54, args[4]);
    put32(rec, 0x58, count - 2);
    put32(rec, 0x64, std::bit_cast<quint32>(length));
    put32(rec, 0x68, 5);
    put8(rec, 0x82, width);
    put8(rec, 0x83, palette);
    put8(rec, 0x84, quint8(args[8]));
    put8(rec, 0x85, 1);
    if (args[7]) {
        const int stop = lordly_ribbon_stop(data(rec));
        int cursor = 0;
        while ((cursor + margins_[width - 1]) * 2 < stop)
            cursor += 2;
        put32(rec, 0x68, cursor);
        put8(rec, 0x85, 0);
    }
    return r;
}
bool SceneRibbons::service(quint32 opcode, const QVector<quint32> &args, int &result,
                           const QMap<int, QPointF> &markers) {
    if (opcode < 0x49 || opcode > 0x4c)
        return false;
    if (opcode == 0x49) {
        auto next = load(args, markers);
        ribbons_[args[0]] = std::move(next);
        return true;
    }
    const int expected = opcode == 0x4a ? 3 : opcode == 0x4b ? 2 : 3;
    if (args.size() != expected)
        throw Error("Ribbon command is truncated");
    const int slot = qint32(args[opcode == 0x4c ? 2 : 0]);
    if (opcode == 0x4b && slot == -1) {
        for (auto &r : ribbons_)
            if (r) {
                if (args[1] == 1)
                    put8(r->record, 0x86, 1);
                else
                    r.reset();
            }
        return true;
    }
    if (slot < 0 || slot >= 20)
        throw Error("Ribbon slot is outside zero through 19");
    auto &r = ribbons_[slot];
    if (opcode == 0x4b) {
        if (r) {
            if (args[1] == 1)
                put8(r->record, 0x86, 1);
            else
                r.reset();
        }
        return true;
    }
    if (!r)
        throw Error("Ribbon command requires an existing path");
    if (opcode == 0x4c)
        result = u8(r->record, 0x85);
    else {
        endpoint(*r, qint32(args[1]), qint32(args[2]));
        lordly_ribbon_endpoint(data(r->record), qint32(args[1]), qint32(args[2]));
    }
    return true;
}
void SceneRibbons::advance() {
    for (auto &r : ribbons_)
        if (r && !lordly_ribbon_advance(data(r->record), margins_.data()))
            r.reset();
}
bool SceneRibbons::empty() const {
    for (const auto &r : ribbons_)
        if (r)
            return false;
    return true;
}
QImage SceneRibbons::render(const SceneCamera &camera, const QByteArray &context) const {
    if (context.size() != 0x848)
        throw Error("Ribbon drawing needs the image presentation context");
    const float sx = std::bit_cast<float>(u32(context, 0x818)),
                sy = std::bit_cast<float>(u32(context, 0x81c));
    float matrix[16];
    uint8_t packed[64];
    lordly_matrix_scale(matrix, sx, sy, .5f);
    if (!lordly_matrix_pack(packed, matrix))
        throw Error("Ribbon projection exceeds fixed-point bounds");
    const QByteArray parent(reinterpret_cast<const char *>(packed), 64);
    QImage image(320, 240, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor colors[]{QColor(100, 120, 250), QColor(255, 50, 50), QColor(50, 255, 100),
                          QColor(200, 100, 180)};
    for (const auto &optional : ribbons_)
        if (optional) {
            const auto &r = *optional;
            const int count = qMin(int(r.samples.size()), int(u32(r.record, 0x68) + 1) / 2);
            const int fade = u8(r.record, 0x86);
            const int erased = fade ? (int(r.samples.size()) * 2 / 30) * fade : 0;
            const int width = u8(r.record, 0x82) + 3;
            QColor color = colors[u8(r.record, 0x83)];
            for (int i = qMax(1, erased / 2); i < count; ++i) {
                const auto a = camera.projectPackedPoint(
                    parent, {r.samples[i - 1].x(), -r.samples[i - 1].y(), 0});
                const auto b =
                    camera.projectPackedPoint(parent, {r.samples[i].x(), -r.samples[i].y(), 0});
                if (!a.visible || !b.visible)
                    continue;
                color.setAlpha(u8(r.record, 0x84) ? 112 * i / qMax(1, count) : 112);
                QPen pen(color, 2 * width * std::abs(a.pixelsPerUnit * sx), Qt::SolidLine,
                         Qt::RoundCap);
                painter.setPen(pen);
                painter.drawLine(a.screen, b.screen);
            }
        }
    return image;
}
} // namespace lordly
