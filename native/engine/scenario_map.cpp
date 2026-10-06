#include "engine/scenario_map.h"
#include "engine/archive.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <algorithm>
#include <bit>
#include <cmath>

namespace lordly {
namespace {
struct ImageAlignment {
    QByteArray sourceHash;
    QSize size;
    QRectF bounds;
};
const QMap<int, ImageAlignment> &imageAlignments() {
    static const auto values = [] {
        QFile file(":/game/scenarios/map-images.json");
        if (!file.open(QIODevice::ReadOnly))
            throw Error("Missing map image calibration");
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object["schema"].toInt() != 1 || !object["maps"].isArray())
            throw Error("Invalid map image calibration");
        QMap<int, ImageAlignment> result;
        for (const auto &value : object["maps"].toArray()) {
            const auto row = value.toObject();
            const int key = row["scenario"].toInt();
            const auto hash = row["imageSha256"].toString().toLatin1();
            const auto b = row["worldBounds"].toArray();
            const QSize size(row["width"].toInt(), row["height"].toInt());
            if (key < 1 || key > 64 || result.contains(key) || hash.size() != 64 ||
                QByteArray::fromHex(hash).toHex().toUpper() != hash || b.size() != 4 ||
                size.isEmpty() || size.width() > 8192 || size.height() > 8192)
                throw Error("Invalid map image calibration row");
            for (const auto &coordinate : b)
                if (!coordinate.isDouble() || !std::isfinite(coordinate.toDouble()) ||
                    std::abs(coordinate.toDouble()) > 10000)
                    throw Error("Invalid calibrated map coordinate");
            const QRectF bounds(QPointF(b[0].toDouble(), b[1].toDouble()),
                                QPointF(b[2].toDouble(), b[3].toDouble()));
            if (bounds.width() <= 0 || bounds.height() <= 0)
                throw Error("Empty calibrated map rectangle");
            result.insert(key, {hash, size, bounds});
        }
        return result;
    }();
    return values;
}
quint32 le32(const QByteArray &data, qsizetype at) {
    return quint32(u8(data, at)) | (quint32(u8(data, at + 1)) << 8) |
           (quint32(u8(data, at + 2)) << 16) | (quint32(u8(data, at + 3)) << 24);
}
// The original -4 loader accepts compact headers in these collections rather
// than ordinary named LHA headers: mode, stored LE32, decoded LE32, payload.
QVector<QByteArray> collection(const QByteArray &rom, quint32 root) {
    const quint64 end = quint64(root) + 4 + u32(rom, root);
    if (end > quint64(rom.size()) || end <= root + 4 || end - root > 0x10000)
        throw Error("Map collection exceeds its resource envelope");
    quint64 at = root + 4;
    QVector<QByteArray> result;
    while (at < end - 1) {
        if (at + 9 > end - 1 || result.size() == 128)
            throw Error("Truncated map resource header");
        const int mode = u8(rom, at);
        const quint32 stored = le32(rom, at + 1), size = le32(rom, at + 5);
        if ((mode != 0x80 && mode != 0x85) || !stored || !size || size > 0x10000 ||
            at + 9 + stored > end - 1 || (mode == 0x80 && stored != size))
            throw Error("Unsupported or damaged map resource");
        const auto payload = rom.mid(at + 9, stored);
        result << (mode == 0x80 ? payload : decodeLh5(payload, size));
        at += 9 + stored;
    }
    if (at != end - 1 || u8(rom, at) || result.isEmpty())
        throw Error("Invalid map collection terminator");
    return result;
}
float real(const QByteArray &bytes, int at) {
    const float value = std::bit_cast<float>(u32(bytes, at));
    if (!std::isfinite(value) || std::abs(value) > 10000)
        throw Error("Map coordinate is not a finite world position");
    return value;
}
} // namespace
ScenarioMap ScenarioMap::load(const Rom &rom, int key) {
    rom.requireSupported();
    if (rom.revision() != 0 || key < 1 || key > 64)
        throw Error("Map preview requires a US revision 0 scenario");
    const auto &data = rom.bytes();
    const auto row = 0x654a0 + key * 9;
    const auto rectangles = collection(data, 0x262610c);
    const auto placements = collection(data, 0x2626786);
    const int rectangle = u8(data, row + 4), pointSet = u8(data, row + 7);
    if (rectangle >= rectangles.size() || pointSet >= placements.size())
        throw Error("Map selector exceeds its collection");
    const auto &r = rectangles[rectangle], &p = placements[pointSet];
    if (r.size() != 32 || p.size() < 28 || u32(p, 24) > 255 || p.size() != 28 + u32(p, 24) * 8)
        throw Error("Map bounds or placement table has an unexpected size");
    ScenarioMap result;
    if (key == 10 || key == 22 || key == 51 || key == 54 || key == 64)
        result.placementNotice = "Initial ROM points differ from the earlier editor's placement "
                                 "reference for this scenario. The difference is unresolved.";
    result.bounds = QRectF(QPointF(real(r, 16), real(r, 20)), QPointF(real(r, 24), real(r, 28)));
    if (result.bounds.width() <= 0 || result.bounds.height() <= 0)
        throw Error("Map bounds are reversed or empty");
    for (int i = 0; i < int(u32(p, 24)); ++i)
        result.points << QPointF(real(p, 28 + i * 8), real(p, 32 + i * 8));
    // Original image caller at z64 0x2B50D0..F0: nine-byte scenario row,
    // signed byte +3 minus four, resource key 0x018BD022.
    const int imageIndex = qint8(u8(data, row + 3)) - 4;
    constexpr quint32 root = 0x1e512a2;
    if (u32(data, root) != 41 * 4 || (imageIndex < 0 && imageIndex != -4) || imageIndex >= 41)
        throw Error("Map image selector exceeds its directory");
    // Internal scenario key 10 selects the non-map model (byte +3 is zero).
    if (imageIndex < 0)
        return result;
    const quint32 imageKey = u32(data, root + 4 + imageIndex * 4);
    if (imageKey) {
        if (imageKey >= 0x10000000 || imageKey % 2)
            throw Error("Invalid map image resource key");
        const quint32 entry = imageKey + 0x594280;
        const quint64 end = quint64(entry) + 4 + u32(data, entry);
        const auto member = LhaMember::read(data, entry + 4);
        if (end > quint64(data.size()) ||
            quint64(member.offset + member.headerSize + member.storedSize) != end - 1 ||
            u8(data, end - 1) || !member.name.startsWith("s_map"))
            throw Error("Map image archive differs from its directory");
        result.imageName = member.name;
        const auto decoded = member.decode(rom);
        result.image = decodeN64Image(decoded);
        const auto &calibrations = imageAlignments();
        const auto found = calibrations.constFind(key);
        if (found != calibrations.cend() &&
            found->size == QSize(result.image->width, result.image->height) &&
            found->sourceHash == sha256(decoded).toUpper())
            result.imageBounds = found->bounds;
    }
    return result;
}
std::optional<QRectF> ScenarioMap::imageGridBounds() const {
    if (!imageBounds)
        return {};
    if (bounds.width() <= 0 || bounds.height() <= 0)
        throw Error("Invalid map bounds");
    const auto point = [&](QPointF p) {
        return QPointF((p.x() - bounds.left()) / bounds.width() * 256.,
                       (p.y() - bounds.top()) / bounds.height() * 256.);
    };
    return QRectF(point(imageBounds->topLeft()), point(imageBounds->bottomRight()));
}
QPointF ScenarioMap::world(int x, int z) const {
    if (x < 0 || x > 255 || z < 0 || z > 255)
        throw Error("Map coordinates must fit unsigned bytes");
    return {bounds.left() + bounds.width() * x / 256., bounds.top() + bounds.height() * z / 256.};
}
QPoint ScenarioMap::bytes(QPointF position, bool selectorEncoding) const {
    if (!std::isfinite(position.x()) || !std::isfinite(position.y()) || bounds.width() <= 0 ||
        bounds.height() <= 0)
        throw Error("Invalid map coordinate conversion");
    // Clamp before converting to an integer, including far outside the view.
    return {int(std::round(
                std::clamp((position.x() - bounds.left()) / bounds.width() * 256., 0., 255.))),
            int(std::round(std::clamp((position.y() - bounds.top()) / bounds.height() * 256.,
                                      selectorEncoding ? 1. : 0., 255.)))};
}
std::optional<QPointF> ScenarioMap::placement(int x, int z) const {
    if (z)
        return world(x, z);
    if (x < 1 || x > points.size())
        return {};
    return points[x - 1];
}
} // namespace lordly
