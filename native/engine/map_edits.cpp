#include "engine/map_edits.h"
#include <QJsonArray>
#include <QLineF>
#include <QSet>
#include <algorithm>
#include <bit>
#include <cmath>
namespace lordly {
namespace {
constexpr quint32 geometryRoot = 0x1e39dce, terrainRoot = 0x20712ea, pointsKey = 0x2092506;
void real(QByteArray &b, int at, double v) {
    if (!std::isfinite(v) || std::abs(v) > 10000)
        throw Error("Invalid site position");
    put32(b, at, std::bit_cast<quint32>(float(v)));
}
} // namespace
MapEdits MapEdits::load(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Map editing requires US revision 0");
    MapEdits out;
    out.rom_ = rom;
    return out;
}
int MapEdits::modelId(int key) const {
    if (key < 1 || key > 64 || key == 54)
        throw Error("Choose a supported scenario map");
    const int id = u8(rom_.bytes(), 0x654a0 + key * 9 + 3);
    if (id < 4 || id > 44)
        throw Error("This internal key has no editable battlefield");
    return id;
}
const FieldMapSource &MapEdits::source(int key) const {
    const int id = modelId(key);
    if (sources_.contains(id))
        return sources_[id];
    const int row = 0x654a0 + key * 9;
    FieldMapSource s;
    s.key = key;
    s.modelId = id;
    s.map = ScenarioMap::load(rom_, key);
    s.geometryIndex = int(u32(rom_.bytes(), 0x65710 + id * 12));
    s.terrainIndex = u8(rom_.bytes(), row + 5);
    s.pointIndex = u8(rom_.bytes(), row + 7);
    for (int shared : sharedScenarios(key)) {
        const int other = 0x654a0 + shared * 9;
        if (u8(rom_.bytes(), other + 4) != u8(rom_.bytes(), row + 4) ||
            u8(rom_.bytes(), other + 5) != s.terrainIndex ||
            u8(rom_.bytes(), other + 7) != s.pointIndex)
            throw Error("Shared model has different gameplay maps; this ROM needs separate map "
                        "ownership before editing");
    }
    s.geometry = fieldMembers(fieldResource(rom_, fieldChild(rom_, geometryRoot, s.geometryIndex)));
    s.terrain = fieldMembers(fieldResource(rom_, fieldChild(rom_, terrainRoot, s.terrainIndex)));
    const auto places = fieldMembers(fieldResource(rom_, pointsKey));
    if (s.pointIndex >= places.size() || s.terrain.size() < 2 ||
        s.terrain[0].data.size() != 65536 || s.terrain[1].data.size() != 4096)
        throw Error("Map gameplay grids have an unsupported layout");
    s.points = places[s.pointIndex].data;
    s.field = Battlefield::load(rom_, key);
    sources_[id] = s;
    return sources_[id];
}
const FieldChanges &MapEdits::changes(int key) const {
    static const FieldChanges empty;
    const auto i = edits_.constFind(modelId(key));
    return i == edits_.cend() ? empty : i.value();
}
QVector<int> MapEdits::sharedScenarios(int key) const {
    const int id = modelId(key);
    QVector<int> out;
    for (int k = 1; k <= 64; ++k)
        if (u8(rom_.bytes(), 0x654a0 + k * 9 + 3) == id)
            out << k;
    return out;
}
QVector<QPointF> MapEdits::points(int key) const {
    auto out = source(key).map.points;
    for (auto i = changes(key).sites.begin(); i != changes(key).sites.end(); ++i)
        out[i.key()] = i.value();
    return out;
}
bool MapEdits::canMoveSite(int key, int row) const {
    const auto &s = source(key);
    return row >= 0 && row < s.map.points.size() && s.field.siteGroups().contains(row + 1);
}
void MapEdits::moveSite(int key, int row, QPointF point) {
    const auto &s = source(key);
    if (!canMoveSite(key, row))
        throw Error("This site has no independently identified building group yet");
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !s.map.bounds.contains(point))
        throw Error("Keep the site inside the map");
    auto next = edits_.value(s.modelId);
    if (QLineF(point, s.map.points[row]).length() < .0001)
        next.sites.remove(row);
    else
        next.sites[row] = point;
    // Compile before committing, so unsupported shared display-list state cannot
    // leave a marker moved without a corresponding building edit.
    editFieldModel(s, next, {});
    edits_[s.modelId] = next;
    prune(s.modelId);
}
void MapEdits::paint(int key, const QVector<QPointF> &stroke, int terrain, int radius) {
    const auto &s = source(key);
    if (stroke.isEmpty() || stroke.size() > 10000 || radius < 1 || radius > 32 ||
        (terrain != 255 && (terrain < 1 || terrain > 14)))
        throw Error("Invalid terrain brush");
    auto next = edits_.value(s.modelId);
    auto stamp = [&](QPointF p) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || std::abs(p.x()) > 4096 ||
            std::abs(p.y()) > 4096)
            throw Error("Invalid brush position");
        const int cx = int(std::round(p.x())), cy = int(std::round(p.y()));
        for (int y = std::max(0, cy - radius); y <= std::min(255, cy + radius); ++y)
            for (int x = std::max(0, cx - radius); x <= std::min(255, cx + radius); ++x) {
                if ((x - p.x()) * (x - p.x()) + (y - p.y()) * (y - p.y()) > radius * radius)
                    continue;
                if (!u8(s.terrain[0].data, y * 256 + x) || u8(s.terrain[0].data, y * 256 + x) >= 15)
                    continue;
                next.paint[y * 256 + x] = char(terrain);
            }
    };
    stamp(stroke.first());
    for (int i = 1; i < stroke.size(); ++i) {
        const int n = int(std::ceil(QLineF(stroke[i - 1], stroke[i]).length()));
        if (n > 8192)
            throw Error("Brush path exceeds map");
        for (int j = 1; j <= n; ++j)
            stamp(stroke[i - 1] + (stroke[i] - stroke[i - 1]) * double(j) / n);
    }
    edits_[s.modelId] = next;
    prune(s.modelId);
}
void MapEdits::prune(int id) {
    const auto &s = edits_[id];
    if (s.sites.isEmpty() && s.paint == QByteArray(65536, char(-1)))
        edits_.remove(id);
}
void MapEdits::restore(int key) {
    edits_.remove(modelId(key));
}
QByteArray MapEdits::fineTerrain(int key) const {
    auto out = source(key).terrain[0].data;
    const auto &p = changes(key).paint;
    for (int i = 0; i < 65536; ++i)
        if (u8(p, i) != 255)
            out[i] = p[i];
    return out;
}
QImage MapEdits::texture(int code) const {
    if (textures_.contains(code))
        return textures_[code];
    QSet<int> visited;
    for (int key = 1; key <= 64; ++key) {
        if (key == 54)
            continue;
        const int id = u8(rom_.bytes(), 0x654a0 + key * 9 + 3);
        if (id < 4 || visited.contains(id))
            continue;
        visited.insert(id);
        const auto &s = source(key);
        // Stock maps classify snowy bridges as ordinary bridges. Use their
        // artwork for authored type 10 while retaining its separate game rules.
        const int sampledType = code == 10 ? 2 : code;
        if (code == 10 && !s.terrain[0].data.contains(char(11)))
            continue;
        if (!s.terrain[0].data.contains(char(sampledType)))
            continue;
        auto image = s.field.terrainTexture(sampledType, s.terrain[0].data, s.map.bounds);
        if (!image.isNull()) {
            textures_[code] = image;
            return image;
        }
    }
    throw Error(QString("No paint texture for terrain %1 was found in the loaded ROM").arg(code));
}
QByteArray MapEdits::model(int key) const {
    const auto &s = source(key);
    const auto &c = changes(key);
    if (!edits_.contains(s.modelId))
        return s.geometry[0].data;
    QMap<int, QImage> tiles;
    for (unsigned char type : c.paint)
        if (type != 255 && !tiles.contains(type))
            tiles[type] = texture(type);
    return editFieldModel(s, c, tiles);
}
QVector<FieldPlan> MapEdits::plan() const {
    QVector<FieldPlan> out;
    for (auto e = edits_.begin(); e != edits_.end(); ++e) {
        int key = 0;
        for (int k = 1; k <= 64; ++k)
            if (u8(rom_.bytes(), 0x654a0 + k * 9 + 3) == e.key()) {
                key = k;
                break;
            }
        const auto &s = source(key);
        auto geometry = s.geometry, terrain = s.terrain;
        auto ps = s.points;
        geometry[0].data = model(key);
        terrain[0].data = fineTerrain(key);
        // Each coarse 4x4 area uses its dominant authored type; unpainted areas
        // keep the original hand-authored battle/encounter classification.
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                if (u8(s.terrain[1].data, y * 64 + x) >= 15)
                    continue;
                int votes[15]{};
                for (int dy = 0; dy < 4; ++dy)
                    for (int dx = 0; dx < 4; ++dx) {
                        int t = u8(e->paint, (y * 4 + dy) * 256 + x * 4 + dx);
                        if (t < 15)
                            ++votes[t];
                    }
                int best = 0;
                for (int t = 1; t < 15; ++t)
                    if (votes[t] > votes[best])
                        best = t;
                if (best)
                    put8(terrain[1].data, y * 64 + x, quint8(best));
            }
        // Alternate movement grids on the fortified maps use the same raster
        // coordinates; preserve their untouched gates/flags and paint only cells.
        for (int n = 2; n < terrain.size(); ++n)
            if (terrain[n].data.size() == 65536)
                for (int i = 0; i < 65536; ++i)
                    if (u8(e->paint, i) != 255 && u8(s.terrain[n].data, i) > 0 &&
                        u8(s.terrain[n].data, i) < 15)
                        terrain[n].data[i] = e->paint[i];
        for (auto p = e->sites.begin(); p != e->sites.end(); ++p) {
            real(ps, 28 + p.key() * 8, p->x());
            real(ps, 32 + p.key() * 8, p->y());
        }
        out << FieldPlan{s.geometryIndex,
                         s.terrainIndex,
                         s.pointIndex,
                         encodeFieldMembers(geometry),
                         encodeFieldMembers(terrain),
                         ps};
    }
    return out;
}
QByteArray MapEdits::rebuild(const Rom &rom) const {
    auto image = rom.bytes();
    const auto plans = plan();
    for (const auto &p : plans) {
        replaceFieldChild(image, geometryRoot, p.geometryIndex, p.geometry);
        replaceFieldChild(image, terrainRoot, p.terrainIndex, p.terrain);
        replaceFieldMemberInPlace(image, pointsKey, p.pointIndex, p.points);
    }
    const auto reopened = Rom::fromBytes(image);
    for (const auto &p : plans) {
        if (fieldResource(reopened, fieldChild(reopened, geometryRoot, p.geometryIndex)) !=
                p.geometry ||
            fieldResource(reopened, fieldChild(reopened, terrainRoot, p.terrainIndex)) !=
                p.terrain ||
            fieldMembers(fieldResource(reopened, pointsKey))[p.pointIndex].data != p.points)
            throw Error("Exported map resource does not match the edited map");
    }
    return image;
}
QJsonObject MapEdits::json() const {
    QJsonObject maps;
    for (auto i = edits_.begin(); i != edits_.end(); ++i) {
        QJsonObject sites;
        for (auto p = i->sites.begin(); p != i->sites.end(); ++p)
            sites[QString::number(p.key())] = QJsonArray{p->x(), p->y()};
        maps[QString::number(i.key())] =
            QJsonObject{{"paint", QString::fromLatin1(i->paint.toBase64())}, {"sites", sites}};
    }
    return {{"schema", 1}, {"maps", maps}};
}
void MapEdits::apply(const QJsonObject &o) {
    if (o["schema"].toInt() != 1 || !o["maps"].isObject())
        throw Error("Invalid saved map edits");
    auto next = *this;
    next.edits_.clear();
    const auto maps = o["maps"].toObject();
    for (auto i = maps.begin(); i != maps.end(); ++i) {
        bool ok;
        int id = i.key().toInt(&ok);
        int key = 0;
        for (int k = 1; k <= 64; ++k)
            if (u8(rom_.bytes(), 0x654a0 + k * 9 + 3) == id && k != 54) {
                key = k;
                break;
            }
        if (!ok || !key || !i->isObject())
            throw Error("Invalid saved map identity");
        const auto &s = next.source(key);
        const auto row = i->toObject();
        const auto data = QByteArray::fromBase64(row["paint"].toString().toLatin1(),
                                                 QByteArray::AbortOnBase64DecodingErrors);
        if (data.size() != 65536)
            throw Error("Saved terrain mask must contain 256 by 256 cells");
        for (unsigned char t : data)
            if (t != 255 && (t < 1 || t > 14))
                throw Error("Invalid painted terrain code");
        FieldChanges change;
        change.paint = data;
        if (!row["sites"].isObject())
            throw Error("Invalid saved site positions");
        const auto sites = row["sites"].toObject();
        for (auto p = sites.begin(); p != sites.end(); ++p) {
            int n = p.key().toInt(&ok);
            const auto xy = p->toArray();
            if (!ok || !next.canMoveSite(key, n) || xy.size() != 2 || !xy[0].isDouble() ||
                !xy[1].isDouble() ||
                !s.map.bounds.contains(QPointF(xy[0].toDouble(), xy[1].toDouble())))
                throw Error("Invalid saved town position");
            change.sites[n] = {xy[0].toDouble(), xy[1].toDouble()};
        }
        next.edits_[id] = change;
        next.prune(id);
    }
    *this = std::move(next);
}
} // namespace lordly
