#include "engine/map_edits.h"
#include "engine/field_borders.h"
#include "engine/field_brush.h"
#include <QImage>
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
int peakTerrain(const FieldMapSource &s, const FieldChanges &c, QPointF p) {
    if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || p.x() < 0 || p.y() < 0 || p.x() >= 256 ||
        p.y() >= 256)
        return -1;
    const int at = int(p.y()) * 256 + int(p.x());
    return u8(c.paint, at) == 255 ? u8(s.visualTerrain, at) : u8(c.paint, at);
}
void validatePeaks(const FieldMapSource &s, const FieldChanges &c) {
    auto base = c;
    base.peaks.clear();
    const auto defaults = fieldMountainPeaks(s, base);
    QMap<int, std::shared_ptr<FieldBrushShape>> shapes;
    if (c.peaks.size() > 4096)
        throw Error("Too many mountain peaks");
    for (int i = 0; i < c.peaks.size(); ++i) {
        const auto &p = c.peaks[i];
        if (!std::isfinite(p.position.x()) || !std::isfinite(p.position.y()) ||
            !std::isfinite(p.height) || p.height < 0 || p.height > 8 ||
            (p.terrain != 7 && p.terrain != 14) || p.addedId < 0 || p.addedId > 1000000 ||
            (p.addedId && p.removed))
            throw Error("Invalid mountain peak height or identity");
        if (!p.addedId && std::none_of(defaults.begin(), defaults.end(),
                                       [&](const MountainPeak &q) { return p.sameIdentity(q); }))
            throw Error("This default peak no longer exists on the current terrain");
        if (!p.removed && peakTerrain(s, c, p.position) != p.terrain)
            throw Error("Place peaks inside matching Highlands terrain");
        if (!p.removed && (p.addedId || p.position != p.origin)) {
            if (!shapes.contains(p.terrain))
                shapes[p.terrain] = std::make_shared<FieldBrushShape>(
                    c.paint, p.terrain, s.map.bounds, s.visualTerrain);
            if (shapes[p.terrain]->distance(p.position) <= 0)
                throw Error("Keep peaks inside the mountain outline");
        }
        for (int j = 0; j < i; ++j)
            if (p.sameIdentity(c.peaks[j]))
                throw Error("Duplicate mountain peak identity");
    }
}
// Keep the native image dimensions and CI8 storage layout. Rendering is
// downsampled before palette matching, so narrow roads and small buildings survive.
QByteArray briefing(const FieldMapSource &s, const Battlefield &field, const Rom &rom) {
    const auto imageKey = fieldChild(rom, 0x18bd022, s.modelId - 4);
    const auto member = LhaMember::read(rom.bytes(), imageKey + 0x594284);
    auto data = member.decode(rom);
    const int w = u16(data, 4), h = u16(data, 6);
    if (data.first(4) != QByteArray::fromHex("36340201"))
        throw Error("This briefing image needs a supported CI8 encoder");
    const int stride = (w + 7) & ~7, end = 8 + stride * h;
    if (data.size() < end + 512)
        throw Error("Briefing palette is truncated");
    const auto bounds = s.map.imageBounds.value_or(s.map.bounds);
    const auto rendered = field.render(bounds, std::min(2048, std::max(w, h) * 3))
                              .scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // Preserve the game's established map palette and native image format.
    std::array<QColor, 256> palette;
    for (int i = 0; i < 256; ++i) {
        const auto color = u16(data, end + i * 2);
        palette[i] = QColor(((color >> 11) & 31) * 255 / 31, ((color >> 6) & 31) * 255 / 31,
                            ((color >> 1) & 31) * 255 / 31, color & 1 ? 255 : 0);
    }
    QMap<QRgb, quint8> cache;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const auto c = rendered.pixelColor(x, y);
            if (!cache.contains(c.rgb())) {
                int best = 0, error = INT_MAX;
                for (int i = 0; i < 256; ++i) {
                    if (!palette[i].alpha())
                        continue;
                    const int r = c.red() - palette[i].red(), g = c.green() - palette[i].green(),
                              b = c.blue() - palette[i].blue();
                    const int difference = r * r + g * g * 2 + b * b;
                    if (difference < error) {
                        best = i;
                        error = difference;
                    }
                }
                cache[c.rgb()] = quint8(best);
            }
            put8(data, 8 + y * stride + x, cache[c.rgb()]);
        }
    // Native readers stop at pixels/palette. A small editor footer preserves the
    // same framing when a generated ROM is reopened and edited again.
    data.truncate(end + 512);
    const int at = int(data.size());
    data += QByteArray("LCMAP001", 8) + QByteArray(16, 0);
    real(data, at + 8, bounds.left());
    real(data, at + 12, bounds.top());
    real(data, at + 16, bounds.right());
    real(data, at + 20, bounds.bottom());
    return member.rebuild(rom, data) + char(0);
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
    s.visualTerrain = s.field.terrainGrid(s.terrain[0].data, s.map.bounds);
    s.terrainFaces = s.field.terrainFaces(s.terrain[0].data, s.map.bounds);
    const int edge = 0x13c874 + key * 4;
    s.boundary = QRectF(QPointF(u8(rom_.bytes(), edge), u8(rom_.bytes(), edge + 1)),
                        QPointF(u8(rom_.bytes(), edge + 2), u8(rom_.bytes(), edge + 3)));
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
    editFieldModel(s, next, {}, next.city.isEmpty() ? QVector<CityAsset>{} : cityAssets());
    edits_[s.modelId] = next;
    prune(s.modelId);
}
void MapEdits::paint(int key, const QVector<QPointF> &stroke, int terrain, double radius) {
    const auto &s = source(key);
    if (stroke.isEmpty() || stroke.size() > 10000 || !std::isfinite(radius) || radius < 1 ||
        radius > 32 || (terrain != 255 && !cityPaint(terrain) && (terrain < 1 || terrain > 14)))
        throw Error("Invalid terrain brush");
    auto next = edits_.value(s.modelId);
    auto stamp = [&](QPointF p) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || std::abs(p.x()) > 4096 ||
            std::abs(p.y()) > 4096)
            throw Error("Invalid brush position");
        for (int y = std::max(0, int(std::ceil(p.y() - radius)));
             y <= std::min(255, int(std::floor(p.y() + radius))); ++y)
            for (int x = std::max(0, int(std::ceil(p.x() - radius)));
                 x <= std::min(255, int(std::floor(p.x() + radius))); ++x) {
                if ((x - p.x()) * (x - p.x()) + (y - p.y()) * (y - p.y()) > radius * radius)
                    continue;
                if (u8(s.terrain[0].data, y * 256 + x) >= 15)
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
    if (!next.peaks.isEmpty()) {
        auto base = next;
        base.peaks.clear();
        const auto available = fieldMountainPeaks(s, base);
        QMap<int, std::shared_ptr<FieldBrushShape>> shapes;
        next.peaks.removeIf([&](const MountainPeak &p) {
            if (!p.removed && (p.addedId || p.position != p.origin)) {
                if (!shapes.contains(p.terrain))
                    shapes[p.terrain] = std::make_shared<FieldBrushShape>(
                        next.paint, p.terrain, s.map.bounds, s.visualTerrain);
                if (shapes[p.terrain]->distance(p.position) <= 0)
                    return true;
            }
            return (!p.removed && peakTerrain(s, next, p.position) != p.terrain) ||
                   (!p.addedId &&
                    std::none_of(available.begin(), available.end(),
                                 [&](const MountainPeak &q) { return p.sameIdentity(q); }));
        });
    }
    edits_[s.modelId] = next;
    prune(s.modelId);
}
QVector<MountainPeak> MapEdits::mountainPeaks(int key) const {
    return fieldMountainPeaks(source(key), changes(key));
}
MountainPeak MapEdits::currentPeak(int key, const MountainPeak &peak) const {
    const auto available = mountainPeaks(key);
    const auto at = std::find_if(available.begin(), available.end(),
                                 [&](const MountainPeak &p) { return p.sameIdentity(peak); });
    if (at == available.end())
        throw Error("This peak no longer exists. Select a peak on the current terrain.");
    return *at;
}
void MapEdits::savePeak(int key, const MountainPeak &peak) {
    if (!std::isfinite(peak.height) || peak.height < 0 || peak.height > 8)
        throw Error("Peak height must be between 0 and 8");
    const int id = modelId(key);
    auto next = *this;
    auto &peaks = next.edits_[id].peaks;
    peaks.removeIf([&](const MountainPeak &p) { return p.sameIdentity(peak); });
    if (peak.addedId ? !peak.removed
                     : peak.removed || peak.position != peak.origin ||
                           std::abs(peak.height - peak.defaultHeight) > .00001)
        peaks << peak;
    validatePeaks(next.source(key), next.changes(key));
    next.prune(id);
    next.model(key); // Validate drawing allocation before committing the edit.
    editFieldHeight(next.source(key), next.changes(key));
    edits_ = std::move(next.edits_);
}
void MapEdits::setPeakHeight(int key, const MountainPeak &peak, double height) {
    auto p = currentPeak(key, peak);
    p.height = height;
    savePeak(key, p);
}
MountainPeak MapEdits::addPeak(int key, QPointF position, double height) {
    const int type = peakTerrain(source(key), changes(key), position);
    if (type != 7 && type != 14)
        throw Error("Click inside Highlands terrain to add a peak");
    const auto available = mountainPeaks(key);
    for (const auto &p : available)
        if (QLineF(p.position, position).length() < 1)
            throw Error("Keep peaks at least one grid square apart");
    int id = 1;
    for (const auto &p : changes(key).peaks)
        id = std::max(id, p.addedId + 1);
    MountainPeak p{position, type, height, 1.6, position, id};
    savePeak(key, p);
    return p;
}
void MapEdits::movePeak(int key, const MountainPeak &peak, QPointF position) {
    auto p = currentPeak(key, peak);
    for (const auto &other : mountainPeaks(key))
        if (!p.sameIdentity(other) && QLineF(other.position, position).length() < 1)
            throw Error("Keep peaks at least one grid square apart");
    p.position = position;
    savePeak(key, p);
}
void MapEdits::removePeak(int key, const MountainPeak &peak) {
    auto p = currentPeak(key, peak);
    p.removed = true;
    savePeak(key, p);
}
void MapEdits::restorePeaks(int key) {
    const int id = modelId(key);
    if (edits_.contains(id)) {
        edits_[id].peaks.clear();
        prune(id);
    }
}
void MapEdits::prune(int id) {
    const auto &s = edits_[id];
    if (!s.boundary && s.sites.isEmpty() && s.city.isEmpty() && s.peaks.isEmpty() &&
        s.paint == QByteArray(65536, char(-1)))
        edits_.remove(id);
}
void MapEdits::restore(int key) {
    edits_.remove(modelId(key));
}
QRectF MapEdits::boundary(int key) const {
    return changes(key).boundary.value_or(source(key).boundary);
}
void MapEdits::setBoundary(int key, QRectF edge) {
    if (key == 39)
        throw Error(
            "Fort Romulus uses a separate scripted boundary; it cannot be resized here yet");
    const auto &s = source(key);
    edge = QRectF(QPointF(std::round(edge.left()), std::round(edge.top())),
                  QPointF(std::round(edge.right()), std::round(edge.bottom())));
    if (!std::isfinite(edge.left()) || !std::isfinite(edge.top()) || !std::isfinite(edge.right()) ||
        !std::isfinite(edge.bottom()) || edge.left() < 1 || edge.top() < 1 || edge.right() > 254 ||
        edge.bottom() > 254 || edge.width() < 8 || edge.height() < 8)
        throw Error("Keep the boundary between 1 and 254, and at least 8 cells wide and tall");
    auto &change = edits_[s.modelId];
    if (edge == s.boundary)
        change.boundary.reset();
    else
        change.boundary = edge;
    prune(s.modelId);
}
QByteArray MapEdits::fineTerrain(int key) const {
    const auto &s = source(key);
    auto out = s.terrain[0].data;
    const auto &c = changes(key);
    for (int i = 0; i < 65536; ++i) {
        const int old = u8(out, i), painted = u8(c.paint, i);
        if (old > 0 && old < 15 && painted != 255)
            out[i] = char(fieldPaintTerrain(painted));
        if (c.boundary) {
            const QPointF p(i % 256, i / 256);
            if (!c.boundary->contains(p))
                out[i] = 0;
            else if (!old) {
                const int visual = u8(s.visualTerrain, i);
                out[i] = char(painted != 255              ? fieldPaintTerrain(painted)
                              : visual > 0 && visual < 15 ? visual
                                                          : 3);
            }
        }
    }
    return out;
}
FieldBrush MapEdits::texture(int code) const {
    if (textures_.contains(code))
        return textures_[code];
    if (cityPaint(code)) {
        const int key = code <= 33 ? 47 : code == 34 ? 39 : 51;
        const auto &s = source(key);
        auto brush = code <= 33 ? s.field.groupBrush(1, s.map.bounds)
                                : s.field.terrainBrush(1, s.terrain[0].data, s.map.bounds);
        if (brush.texture.isNull())
            throw Error("The selected city paving is unavailable in this ROM");
        // Broad paving repeats the stones, not both curb edges of every road strip.
        if (code == 32 && brush.texture.width() >= 16)
            brush.texture = brush.texture.copy(brush.texture.width() / 4, 0,
                                               brush.texture.width() / 2, brush.texture.height());
        textures_[code] = brush;
        return brush;
    }
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
        auto image = s.field.terrainBrush(sampledType, s.terrain[0].data, s.map.bounds);
        if (!image.texture.isNull() && ((code != 2 && code != 10) || !image.bridge.isEmpty())) {
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
    QMap<int, FieldBrush> tiles;
    const FieldBorders borders(s, c);
    for (unsigned char type : borders.change.paint)
        if (type != 255 && !tiles.contains(type)) {
            auto brush = cityPaint(type)
                             ? texture(type)
                             : s.field.terrainBrush(type, s.terrain[0].data, s.map.bounds);
            tiles[type] =
                brush.texture.isNull() || ((type == 2 || type == 10) && brush.bridge.isEmpty())
                    ? texture(type)
                    : brush;
        }
    return editFieldModel(s, c, tiles, c.city.isEmpty() ? QVector<CityAsset>{} : cityAssets());
}
const QVector<CityAsset> &MapEdits::cityAssets() const {
    if (!cityAssets_.isEmpty())
        return cityAssets_;
    QVector<CityAsset> result;
    auto add = [&](int key, const char *id, const char *name, QVector<int> groups) {
        result << source(key).field.cityAsset(QString::fromLatin1(id), QString::fromUtf8(name), key,
                                              groups);
    };
    // Explicit complete groups from the stock field programs. House roofs and
    // facades can have separate culling groups; include both as one object.
    add(39, "romulus-house", "Romulus · House", {17, 18});
    add(39, "romulus-house-wide", "Romulus · Wide house", {19, 20, 21});
    add(39, "romulus-house-long", "Romulus · Long house", {32, 33, 34});
    add(39, "romulus-tower", "Romulus · Roofed tower", {26, 27});
    add(39, "romulus-wall", "Romulus · Wall section", {142});
    add(39, "romulus-gate", "Romulus · Gate", {15});
    add(47, "latium-house", "Latium · House", {17});
    add(47, "latium-house-wide", "Latium · Wide house", {69});
    add(47, "latium-house-long", "Latium · Long house", {95});
    add(47, "latium-tower", "Latium · Tower", {148});
    add(47, "latium-gate", "Latium · Gate", {110});
    add(47, "latium-wall", "Latium · Wall section", {123});
    add(47, "latium-keep", "Latium · Keep", {59});
    add(51, "talpaea-house", "Talpaea · House", {84});
    add(51, "talpaea-house-wide", "Talpaea · Wide house", {82});
    add(51, "talpaea-house-long", "Talpaea · Long house", {79});
    add(51, "talpaea-gatehouse", "Talpaea · Gatehouse", {117});
    add(51, "talpaea-keep", "Talpaea · Keep", {1, 2});
    add(51, "talpaea-wall", "Talpaea · Outer wall", {22});
    cityAssets_ = result;
    return cityAssets_;
}
const CityAsset &MapEdits::cityAsset(const QString &id) const {
    for (const auto &asset : cityAssets())
        if (asset.id == id)
            return asset;
    throw Error("Unknown city piece");
}
void MapEdits::placeCity(int key, const QString &asset, QPointF position, int rotation) {
    const auto &s = source(key);
    cityAsset(asset);
    if (changes(key).city.size() >= 256)
        throw Error("This map has reached its 256 city-piece limit");
    auto next = *this;
    next.edits_[s.modelId].city << CityPlacement{asset, position, rotation};
    next.moveCity(key, int(next.edits_[s.modelId].city.size()) - 1, position, rotation);
    *this = std::move(next);
}
void MapEdits::moveCity(int key, int row, QPointF position, int rotation) {
    const auto &s = source(key);
    const auto &pieces = changes(key).city;
    if (row < 0 || row >= pieces.size() || rotation < 0 || rotation >= 360 ||
        !std::isfinite(position.x()) || !std::isfinite(position.y()))
        throw Error("Invalid city-piece placement");
    const auto &asset = cityAsset(pieces[row].asset);
    const double a = rotation * 3.141592653589793 / 180.;
    for (const auto &face : asset.faces)
        for (const auto &v : face.vertices) {
            const QPointF p =
                position + QPointF(v.point.x() * std::cos(a) - v.point.z() * std::sin(a),
                                   v.point.x() * std::sin(a) + v.point.z() * std::cos(a));
            if (!s.map.bounds.contains(p))
                throw Error("Keep the whole city piece inside the visible map");
        }
    edits_[s.modelId].city[row].position = position;
    edits_[s.modelId].city[row].rotation = rotation;
}
void MapEdits::removeCity(int key, int row) {
    const int id = modelId(key);
    if (row < 0 || row >= changes(key).city.size())
        throw Error("Choose a placed city piece");
    edits_[id].city.removeAt(row);
    prune(id);
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
        geometry[1].data = editFieldHeight(s, e.value());
        terrain[0].data = fineTerrain(key);
        // Each coarse 4x4 area uses its dominant authored type; unpainted areas
        // keep the original hand-authored battle/encounter classification.
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                if (!e->boundary && u8(s.terrain[1].data, y * 64 + x) >= 15)
                    continue;
                if (e->boundary) {
                    bool changed = false;
                    for (int dz = 0; dz < 4; ++dz)
                        for (int dx = 0; dx < 4; ++dx) {
                            const int at = (y * 4 + dz) * 256 + x * 4 + dx;
                            changed |= terrain[0].data[at] != s.terrain[0].data[at];
                        }
                    if (!changed)
                        continue;
                }
                int votes[32]{};
                for (int dy = 0; dy < 4; ++dy)
                    for (int dx = 0; dx < 4; ++dx) {
                        int t = fieldPaintTerrain(u8(e->boundary ? terrain[0].data : e->paint,
                                                     (y * 4 + dy) * 256 + x * 4 + dx));
                        if (t < 32 &&
                            (e->boundary || u8(s.terrain[0].data, (y * 4 + dy) * 256 + x * 4 + dx)))
                            ++votes[t];
                    }
                // A partial boundary cut must not turn a fixed wall/gate into
                // ordinary terrain. Only wholly excluded coarse cells become void.
                if (u8(s.terrain[1].data, y * 64 + x) >= 15 && votes[0] != 16)
                    continue;
                int best = 0;
                for (int t = 1; t < 32; ++t)
                    if (votes[t] > votes[best])
                        best = t;
                if (best || e->boundary)
                    put8(terrain[1].data, y * 64 + x, quint8(best));
            }
        // Alternate movement grids on the fortified maps use the same raster
        // coordinates; preserve their untouched gates/flags and paint only cells.
        for (int n = 2; n < terrain.size(); ++n)
            if (terrain[n].data.size() == 65536)
                for (int i = 0; i < 65536; ++i)
                    if (e->boundary && !e->boundary->contains(QPointF(i % 256, i / 256)))
                        terrain[n].data[i] = 0;
                    else if (e->boundary && !u8(s.terrain[n].data, i))
                        terrain[n].data[i] = terrain[0].data[i];
                    else if (u8(e->paint, i) != 255 && u8(s.terrain[n].data, i) > 0 &&
                             u8(s.terrain[n].data, i) < 15)
                        terrain[n].data[i] = char(fieldPaintTerrain(u8(e->paint, i)));
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
    for (auto it = edits_.begin(); it != edits_.end(); ++it) {
        int key = 0;
        for (int k = 1; k <= 64; ++k)
            if (u8(rom_.bytes(), 0x654a0 + k * 9 + 3) == it.key()) {
                key = k;
                break;
            }
        const auto &s = source(key);
        if (s.map.image && (!it->sites.isEmpty() || !it->city.isEmpty() || !it->peaks.isEmpty() ||
                            it->paint != QByteArray(65536, char(-1)))) {
            const auto current = Rom::fromBytes(image);
            const auto field = Battlefield::load(current, key);
            replaceFieldChild(image, 0x18bd022, s.modelId - 4, briefing(s, field, current));
        }
    }
    for (auto it = edits_.begin(); it != edits_.end(); ++it)
        if (it->boundary)
            for (int key = 1; key <= 64; ++key)
                if (u8(rom_.bytes(), 0x654a0 + key * 9 + 3) == it.key()) {
                    const auto b = *it->boundary;
                    const int at = 0x13c874 + key * 4;
                    put8(image, at, quint8(b.left()));
                    put8(image, at + 1, quint8(b.top()));
                    put8(image, at + 2, quint8(b.right()));
                    put8(image, at + 3, quint8(b.bottom()));
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
        if (!i->city.isEmpty()) {
            auto row = maps[QString::number(i.key())].toObject();
            QJsonArray city;
            for (const auto &p : i->city)
                city << QJsonObject{{"asset", p.asset},
                                    {"x", p.position.x()},
                                    {"z", p.position.y()},
                                    {"rotation", p.rotation}};
            row["city"] = city;
            maps[QString::number(i.key())] = row;
        }
        if (i->boundary) {
            auto row = maps[QString::number(i.key())].toObject();
            const auto b = *i->boundary;
            row["boundary"] = QJsonArray{b.left(), b.top(), b.right(), b.bottom()};
            maps[QString::number(i.key())] = row;
        }
        if (!i->peaks.isEmpty()) {
            auto row = maps[QString::number(i.key())].toObject();
            QJsonArray peaks;
            for (const auto &p : i->peaks) {
                QJsonObject item{{"x", p.position.x()},
                                 {"z", p.position.y()},
                                 {"terrain", p.terrain},
                                 {"height", p.height}};
                if (p.addedId)
                    item["addedId"] = p.addedId;
                else if (p.origin != p.position || p.removed)
                    item["origin"] = QJsonArray{p.origin.x(), p.origin.y()};
                if (p.removed)
                    item["removed"] = true;
                peaks << item;
            }
            row["peaks"] = peaks;
            maps[QString::number(i.key())] = row;
        }
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
            if (t != 255 && !cityPaint(t) && (t < 1 || t > 14))
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
        if (row.contains("peaks")) {
            if (!row["peaks"].isArray() || row["peaks"].toArray().size() > 4096)
                throw Error("Invalid saved mountain peaks");
            for (const auto &item : row["peaks"].toArray()) {
                const auto p = item.toObject();
                const double height = p["height"].toDouble(-1);
                const QPointF position(p["x"].toDouble(-1), p["z"].toDouble(-1));
                const int type = p["terrain"].toInt(-1);
                if (!item.isObject() || !p["x"].isDouble() || !p["z"].isDouble() ||
                    !p["height"].isDouble() || (p.contains("removed") && !p["removed"].isBool()) ||
                    (p.contains("addedId") &&
                     (!p["addedId"].isDouble() || p["addedId"].toInt(-1) < 1)))
                    throw Error("Invalid saved mountain peak height or position");
                MountainPeak peak{position,
                                  type,
                                  height,
                                  0,
                                  position,
                                  p["addedId"].toInt(),
                                  p["removed"].toBool()};
                if (p.contains("origin")) {
                    const auto origin = p["origin"].toArray();
                    if (origin.size() != 2 || !origin[0].isDouble() || !origin[1].isDouble() ||
                        peak.addedId)
                        throw Error("Invalid saved mountain peak origin");
                    peak.origin = {origin[0].toDouble(), origin[1].toDouble()};
                }
                change.peaks << peak;
            }
            validatePeaks(s, change);
        }
        next.edits_[id] = change;
        if (row.contains("city")) {
            if (!row["city"].isArray() || row["city"].toArray().size() > 256)
                throw Error("Invalid saved city pieces");
            for (const auto &item : row["city"].toArray()) {
                const auto p = item.toObject();
                if (!item.isObject() || !p["asset"].isString() || !p["x"].isDouble() ||
                    !p["z"].isDouble() || !p["rotation"].isDouble() ||
                    p["rotation"].toInt(-1) != p["rotation"].toDouble())
                    throw Error("Invalid saved city-piece placement");
                next.placeCity(key, p["asset"].toString(), {p["x"].toDouble(), p["z"].toDouble()},
                               p["rotation"].toInt());
            }
        }
        if (row.contains("boundary")) {
            const auto b = row["boundary"].toArray();
            if (b.size() != 4 || !b[0].isDouble() || !b[1].isDouble() || !b[2].isDouble() ||
                !b[3].isDouble())
                throw Error("Invalid saved map boundary");
            next.setBoundary(key, QRectF(QPointF(b[0].toDouble(), b[1].toDouble()),
                                         QPointF(b[2].toDouble(), b[3].toDouble())));
        }
        next.prune(id);
    }
    *this = std::move(next);
}
} // namespace lordly
