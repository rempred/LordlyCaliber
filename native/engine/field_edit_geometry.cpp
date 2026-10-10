#include "engine/field_brush.h"
#include "engine/map_edits.h"
#include <QSet>
#include <algorithm>
#include <bit>
#include <cmath>
namespace lordly {
namespace {
double real(const QByteArray &b, int at) {
    const double v = std::bit_cast<float>(u32(b, at));
    if (!std::isfinite(v))
        throw Error("Invalid map coordinate");
    return v;
}
void putReal(QByteArray &b, int at, double v) {
    put32(b, at, std::bit_cast<quint32>(float(v)));
}
void word(QByteArray &b, quint32 n) {
    const int at = int(b.size());
    b.resize(at + 4);
    put32(b, at, n);
}
void command(QByteArray &b, quint32 a, quint32 c) {
    word(b, a);
    word(b, c);
}
double fixed(const QByteArray &b, int i) {
    return qint16(u16(b, i * 2)) + u16(b, 32 + i * 2) / 65536.;
}
void putFixed(QByteArray &b, int i, double n) {
    if (!std::isfinite(n) || std::abs(n) > 32767)
        throw Error("Moved building exceeds matrix range");
    const auto q = qint32(std::round(n * 65536));
    put16(b, i * 2, quint16(quint32(q) >> 16));
    put16(b, 32 + i * 2, quint16(q));
}
// Original RLE sample reader: z64 F3704..F37D4 (RAM 801A69A4).
// Row offsets are halfwords from +28. FFnn repeats the following height nn times.
quint16 heightSample(const QByteArray &b, int x, int y) {
    const int width = u16(b, 0), height = u16(b, 2);
    if (x < 0 || y < 0 || x >= width || y >= height)
        return 0;
    int at = 28 + u16(b, 28 + y * 2) * 2, cell = 0;
    while (cell < width) {
        const int value = u16(b, at);
        at += 2;
        const int run = value >> 8 == 255 ? value & 255 : 1;
        if (!run || cell + run > width)
            throw Error("Invalid field height run");
        const int h = value >> 8 == 255 ? u16(b, at) : value;
        if (value >> 8 == 255)
            at += 2;
        if (x < cell + run)
            return quint16(h);
        cell += run;
    }
    throw Error("Missing field height sample");
}
double heightAt(const FieldMapSource &source, QPointF p) {
    if (source.geometry.size() < 2)
        return 0;
    const auto &b = source.geometry[1].data;
    const double x = (p.x() - real(b, 4)) / (real(b, 8) - real(b, 4)) * (u16(b, 0) - 1),
                 z = (p.y() - real(b, 20)) / (real(b, 24) - real(b, 20)) * (u16(b, 2) - 1);
    const int ix = int(std::floor(x)), iz = int(std::floor(z));
    const double fx = x - ix, fz = z - iz;
    const double a = heightSample(b, ix, iz), d = heightSample(b, ix + 1, iz + 1);
    const double h = fz >= fx ? a * (1 - fz) + heightSample(b, ix, iz + 1) * (fz - fx) + d * fx
                              : a * (1 - fx) + heightSample(b, ix + 1, iz) * (fx - fz) + d * fz;
    // Native height interpolation at z64 F33C8 uses 0x477EFF00 = 65279.
    return real(b, 12) + (real(b, 16) - real(b, 12)) * h / 65279.;
}
struct Append {
    QByteArray bytes;
    int geometry;
    quint32 add(QByteArray data, int alignment = 8) {
        while (bytes.size() % alignment)
            bytes += char(0);
        const auto at = bytes.size();
        if (at + data.size() > 512 * 1024)
            throw Error("This map has reached its detail capacity (512 KiB); simplify or erase "
                        "some strokes");
        bytes += data;
        return 0x02000000u + quint32(at - geometry);
    }
};
class BuildingClone {
    const QByteArray &original_;
    Append &out_;
    QVector3D delta_;
    QMap<quint32, quint32> lists_, matrices_;
    QSet<quint32> active_;
    int commands_ = 0;

  public:
    BuildingClone(const QByteArray &b, Append &out, QVector3D delta)
        : original_(b), out_(out), delta_(delta) {}
    quint32 clone(quint32 address, int depth = 0) {
        if (lists_.contains(address))
            return lists_[address];
        if (depth > 24 || active_.contains(address) || address >> 24 != 2)
            throw Error("Building uses an unsupported shared drawing list");
        active_.insert(address);
        QByteArray code;
        for (quint32 p = address & 0xffffff;; p += 8) {
            if (++commands_ > 50000)
                throw Error("Building drawing list exceeds budget");
            const auto at = out_.geometry + p;
            const quint32 a = u32(original_, at), b = u32(original_, at + 4);
            quint32 next = b;
            const auto op = a >> 24;
            if (op == 0xda) {
                if (!(a & 2) || b >> 24 != 2)
                    throw Error("Building uses a relative or dynamic transform that cannot move "
                                "independently yet");
                if (!matrices_.contains(b)) {
                    auto m = original_.mid(out_.geometry + (b & 0xffffff), 64);
                    if (m.size() != 64)
                        throw Error("Truncated building matrix");
                    for (int axis = 0; axis < 3; ++axis)
                        putFixed(m, 12 + axis, fixed(m, 12 + axis) + delta_[axis]);
                    matrices_[b] = out_.add(m, 16);
                }
                next = matrices_[b];
            } else if (op == 0xde)
                next = clone(b, depth + 1);
            command(code, a, next);
            if (op == 0xdf || (op == 0xde && (a & 0x10000)))
                break;
        }
        active_.remove(address);
        const auto result = out_.add(code);
        lists_[address] = result;
        return result;
    }
};
struct BrushPoint {
    QPointF grid;
    double distance, influence;
};
using BrushPolygon = QVector<BrushPoint>;
BrushPolygon clip(BrushPolygon points, double boundary, bool above, bool influence = false) {
    BrushPolygon out;
    if (points.isEmpty())
        return out;
    auto coordinate = [&](const BrushPoint &p) { return influence ? p.influence : p.distance; };
    auto prior = points.last();
    bool was = above ? coordinate(prior) >= boundary : coordinate(prior) <= boundary;
    for (const auto &p : points) {
        const bool inside = above ? coordinate(p) >= boundary : coordinate(p) <= boundary;
        if (inside != was) {
            const double f = (boundary - coordinate(prior)) / (coordinate(p) - coordinate(prior));
            out << BrushPoint{prior.grid + (p.grid - prior.grid) * f,
                              prior.distance + (p.distance - prior.distance) * f,
                              prior.influence + (p.influence - prior.influence) * f};
        }
        if (inside)
            out << p;
        prior = p;
        was = inside;
    }
    return out;
}
// Use the original rendered land surface, including its authored slopes. A
// coarse spatial index avoids testing every map triangle for every brush vertex.
class Surface {
    QVector<FieldTriangle> triangles_;
    QVector<QVector<int>> bins_ = QVector<QVector<int>>(1024);
    QRectF bounds_;
    const FieldMapSource &source_;
    int cell(double p, double start, double size) const {
        return std::clamp(int((p - start) / size * 32), 0, 31);
    }

  public:
    Surface(const FieldMapSource &s)
        : triangles_(s.field.surface()), bounds_(s.map.bounds), source_(s) {
        for (int i = 0; i < triangles_.size(); ++i) {
            const auto &t = triangles_[i].points;
            const int x0 = cell(std::min({t[0].x(), t[1].x(), t[2].x()}), bounds_.left(),
                                bounds_.width()),
                      x1 = cell(std::max({t[0].x(), t[1].x(), t[2].x()}), bounds_.left(),
                                bounds_.width()),
                      z0 = cell(std::min({t[0].z(), t[1].z(), t[2].z()}), bounds_.top(),
                                bounds_.height()),
                      z1 = cell(std::max({t[0].z(), t[1].z(), t[2].z()}), bounds_.top(),
                                bounds_.height());
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                    bins_[z * 32 + x] << i;
        }
    }
    double height(QPointF p) const {
        double result = -1e9;
        const int x = cell(p.x(), bounds_.left(), bounds_.width()),
                  z = cell(p.y(), bounds_.top(), bounds_.height());
        for (int i : bins_[z * 32 + x]) {
            const auto &t = triangles_[i].points;
            const double ax = t[1].x() - t[0].x(), az = t[1].z() - t[0].z(),
                         bx = t[2].x() - t[0].x(), bz = t[2].z() - t[0].z(), px = p.x() - t[0].x(),
                         pz = p.y() - t[0].z(), det = ax * bz - bx * az;
            if (std::abs(det) < 1e-9)
                continue;
            const double a = (px * bz - bx * pz) / det, b = (ax * pz - px * az) / det;
            if (a >= -1e-5 && b >= -1e-5 && a + b <= 1.00001)
                result = std::max(result,
                                  t[0].y() + a * (t[1].y() - t[0].y()) + b * (t[2].y() - t[0].y()));
        }
        return result == -1e9 ? heightAt(source_, p) : result;
    }
};
QByteArray textureBytes(const QImage &image) {
    const int w = image.width(), h = image.height();
    if (w < 1 || h < 1 || (w & (w - 1)) || (h & (h - 1)) || w * h > 2048)
        throw Error("Invalid map brush texture dimensions");
    QByteArray out(w * h * 2, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const auto c = image.pixelColor(x, y);
            // Decode/re-encode an RGBA16 texel without losing a level to truncation.
            put16(out, (y * w + x) * 2,
                  quint16(((c.red() * 31 + 127) / 255 << 11) | ((c.green() * 31 + 127) / 255 << 6) |
                          ((c.blue() * 31 + 127) / 255 << 1) | (c.alpha() >= 128)));
        }
    return out;
}
struct PaintVertex {
    QVector3D p, normal;
    QPointF uv;
    quint8 alpha;
};
using PaintTriangle = std::array<PaintVertex, 3>;
QVector<PaintTriangle> mesh(const FieldBrushShape &shape, const FieldBrush &brush,
                            const Surface &surface, bool border) {
    QVector<PaintTriangle> result;
    if (shape.cells.isEmpty())
        return result;
    const int step = shape.road() ? 1 : 2;
    const bool hasBorder = !brush.border.isNull() && shape.forest();
    const auto &texture = border ? brush.border : brush.texture;
    auto vertex = [&](const BrushPoint &p) {
        const auto world = shape.world(p.grid);
        const double lift = shape.rise(p.grid);
        auto height = [&](QPointF xy) { return surface.height(xy) + shape.rise(shape.grid(xy)); };
        constexpr double offset = .12;
        QVector3D normal(
            float(height(world - QPointF(offset, 0)) - height(world + QPointF(offset, 0))),
            float(2 * offset),
            float(height(world - QPointF(0, offset)) - height(world + QPointF(0, offset))));
        normal.normalize();
        QPointF uv(world.x() * brush.densityS, world.y() * brush.densityT);
        if (shape.mountain())
            uv = {(world.x() + world.y()) * 8,
                  -.5 + texture.height() * std::clamp(p.distance / shape.peak(p.grid), 0., 1.)};
        else if (border)
            uv = {(world.x() + world.y()) * 48,
                  -.5 + texture.height() * std::clamp(p.distance / shape.edgeWidth(), 0., 1.)};
        else if (shape.road()) {
            const auto road = shape.roadUv(p.grid);
            uv = {-.5 + texture.width() * road.x(), road.y() * brush.densityT};
        }
        return PaintVertex{
            {float(world.x()), float(surface.height(world) + lift + .03125), float(world.y())},
            normal,
            uv,
            quint8(std::round(shape.opacity(p.grid) * 255))};
    };
    auto polygon = [&](BrushPolygon poly) {
        poly = clip(poly, 0, true, true);
        poly = clip(poly, 0, true);
        if (hasBorder)
            poly = clip(poly, shape.edgeWidth(), !border);
        for (int n = 1; n + 1 < poly.size(); ++n) {
            const auto a = poly[n].grid - poly[0].grid, b = poly[n + 1].grid - poly[0].grid;
            if (std::abs(a.x() * b.y() - a.y() * b.x()) < 1e-7)
                continue;
            const PaintTriangle t{vertex(poly[0]), vertex(poly[n]), vertex(poly[n + 1])};
            if (t[0].alpha || t[1].alpha || t[2].alpha)
                result << t;
        }
    };
    auto patch = [&](int x, int z, int width) {
        const QPointF a(x, z), b(x, z + width), c(x + width, z + width), d(x + width, z);
        const BrushPoint pa{a, shape.distance(a), shape.influence(a)},
            pb{b, shape.distance(b), shape.influence(b)},
            pc{c, shape.distance(c), shape.influence(c)},
            pd{d, shape.distance(d), shape.influence(d)};
        polygon({pa, pb, pc});
        polygon({pa, pc, pd});
    };
    for (int z = std::max(0, shape.cells.top() / step * step);
         z <= std::min(255, shape.cells.bottom() + 1); z += step)
        for (int x = std::max(0, shape.cells.left() / step * step);
             x <= std::min(255, shape.cells.right() + 1); x += step) {
            // Retain a single-cell dab even when all coarse corners miss it.
            if (step == 2 && shape.distance({double(x + 1), double(z + 1)}) > 0 &&
                shape.distance({double(x), double(z)}) <= 0 &&
                shape.distance({double(x + 2), double(z)}) <= 0 &&
                shape.distance({double(x), double(z + 2)}) <= 0 &&
                shape.distance({double(x + 2), double(z + 2)}) <= 0) {
                patch(x, z, 1);
                patch(x + 1, z, 1);
                patch(x, z + 1, 1);
                patch(x + 1, z + 1, 1);
            } else
                patch(x, z, step);
        }
    return result;
}
} // namespace
QByteArray editFieldHeight(const FieldMapSource &source, const FieldChanges &change) {
    const auto &original = source.geometry[1].data;
    QMap<int, std::shared_ptr<FieldBrushShape>> shapes;
    for (int type : {5, 7, 13, 14})
        if (change.paint.contains(char(type)))
            shapes[type] = std::make_shared<FieldBrushShape>(change.paint, type, source.map.bounds,
                                                             source.visualTerrain);
    if (shapes.isEmpty())
        return original;
    const int w = u16(original, 0), h = u16(original, 2);
    if (w < 2 || h < 2 || w > 256 || h > 256 || original.size() < 28 + h * 2)
        throw Error("Unsupported field height grid");
    const Surface surface(source);
    const double minY = real(original, 12), oldMax = real(original, 16);
    if (oldMax < minY || real(original, 8) <= real(original, 4) ||
        real(original, 24) <= real(original, 20))
        throw Error("Invalid field height bounds");
    double maxY = oldMax;
    QVector<double> values(w * h);
    bool changed = false;
    for (int z = 0; z < h; ++z)
        for (int x = 0; x < w; ++x) {
            const QPointF p(
                real(original, 4) + x * (real(original, 8) - real(original, 4)) / (w - 1),
                real(original, 20) + z * (real(original, 24) - real(original, 20)) / (h - 1));
            const double base = minY + (oldMax - minY) * heightSample(original, x, z) / 65279.;
            double value = base;
            for (const auto &shape : shapes) {
                // Native foliage is above its walk surface; its movement grid
                // follows roughly half of the canopy rise. Rock uses the visible
                // surface, including when it is painted over existing foliage.
                const double rise = shape->rise(shape->grid(p));
                if (rise > 0) {
                    value =
                        std::max(value, shape->forest() ? base + rise * .5
                                                        : std::max(base, surface.height(p)) + rise);
                    changed = true;
                }
            }
            values[z * w + x] = value;
            maxY = std::max(maxY, value);
        }
    if (!changed)
        return original;
    // Keep the existing scale whenever possible, preserving untouched samples
    // exactly. When a new summit exceeds it, the complete grid is requantized.
    QByteArray out = original.left(28);
    putReal(out, 16, maxY);
    maxY = real(out, 16);
    out.resize(28 + h * 2);
    for (int z = 0; z < h; ++z) {
        const int offset = (int(out.size()) - 28) / 2;
        if (offset > 65535)
            throw Error("Edited height grid exceeds row offset range");
        put16(out, 28 + z * 2, quint16(offset));
        QVector<quint16> row;
        for (int x = 0; x < w; ++x)
            row << quint16(std::clamp(
                std::round((values[z * w + x] - minY) / (maxY - minY) * 65279.), 0., 65279.));
        for (int x = 0; x < w;) {
            int n = 1;
            while (x + n < w && n < 255 && row[x + n] == row[x])
                ++n;
            auto half = [&](quint16 v) {
                const int at = int(out.size());
                out.resize(at + 2);
                put16(out, at, v);
            };
            if (n >= 3) {
                half(quint16(0xff00 | n));
                half(row[x]);
            } else
                for (int i = 0; i < n; ++i)
                    half(row[x]);
            x += n;
        }
    }
    return out;
}
QByteArray editFieldModel(const FieldMapSource &source, const FieldChanges &change,
                          const QMap<int, FieldBrush> &tiles) {
    const auto &original = source.geometry[0].data;
    Append out{original, int((u32(original, 48) + 15) & ~15u)};
    const auto groups = source.field.siteGroups();
    QMap<int, QVector3D> movement;
    auto elevated = source;
    elevated.geometry[1].data = editFieldHeight(source, change);
    // A building on newly raised ground must rise too, even if its X/Z marker
    // was not dragged during this stroke.
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const int row = it.key() - 1;
        if (row < 0 || row >= source.map.points.size())
            continue;
        const auto from = source.map.points[row], to = change.sites.value(row, from);
        const auto delta = to - from;
        const QVector3D shift(float(delta.x()),
                              float(heightAt(elevated, to) - heightAt(source, from)),
                              float(delta.y()));
        if (shift.length() > .00001)
            movement[it.value()] = shift;
    }
    for (auto it = change.sites.begin(); it != change.sites.end(); ++it)
        if (!groups.contains(it.key() + 1))
            throw Error("Town has no verified drawing group");
    constexpr int widths[] = {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 3, 3, 1,
                              1, 0, 1, 1, 1, 2, 2, 3, 1, 2, 2, 3, 2};
    QByteArray program;
    QMap<int, std::shared_ptr<BuildingClone>> clones;
    const auto transforms = source.field.drawTransforms();
    for (quint32 at = u32(original, 0);;) {
        const int instructionAt = int(at);
        const auto op = u32(original, at);
        at += 4;
        if (!op)
            break;
        if (op > 25)
            throw Error("Unsupported field program");
        word(program, op);
        std::array<quint32, 3> args{};
        for (int n = 0; n < widths[op]; ++n) {
            args[n] = u32(original, at);
            at += 4;
        }
        if (op == 18 || op == 20 || op == 22 || op == 24) {
            const int group = int(args[1]);
            if (movement.contains(group)) {
                if (!clones.contains(group))
                    clones[group] = std::make_shared<BuildingClone>(original, out, movement[group]);
                const auto list = clones[group]->clone(args[0]);
                // Flags and some roofs inherit the transform from an earlier material
                // list. Seed each moved draw with its original incoming transform.
                const auto state = transforms.value(instructionAt);
                auto matrix = state.left(64);
                if (state.size() != 128)
                    throw Error("Missing building transform");
                for (int axis = 0; axis < 3; ++axis)
                    putFixed(matrix, 12 + axis, fixed(matrix, 12 + axis) + movement[group][axis]);
                QByteArray wrapper;
                command(wrapper, 0xda380003, out.add(matrix, 16));
                command(wrapper, 0xde000000, list);
                // Do not leak the translated matrix into the following unmoved draw.
                command(wrapper, 0xda380003, out.add(state.mid(64), 16));
                command(wrapper, 0xdf000000, 0);
                args[0] = out.add(wrapper);
            }
        }
        for (int n = 0; n < widths[op]; ++n)
            word(program, args[n]);
    }
    for (auto i = movement.begin(); i != movement.end(); ++i) {
        if (!clones.contains(i.key()))
            throw Error("Town drawing group is unavailable");
        const int at = int(u32(original, 8)) + i.key() * 96;
        for (int p = 0; p < 8; ++p)
            for (int axis = 0; axis < 3; ++axis)
                putReal(out.bytes, at + p * 12 + axis * 4,
                        real(original, at + p * 12 + axis * 4) + i.value()[axis]);
    }
    if (!tiles.isEmpty()) {
        const Surface surface(source);
        const bool fog = source.field.terrainFog();
        QByteArray matrix(64, 0);
        for (int n = 0; n < 3; ++n)
            putFixed(matrix, n * 5, 1. / 64);
        putFixed(matrix, 15, 1);
        QByteArray dl;
        command(dl, 0xe7000000, 0);
        command(dl, 0xda380003, out.add(matrix, 16));
        // Native land uses lighting and vertex normals. Full-white vertex
        // colours bypassed that lighting and washed out the forest in game.
        command(dl, 0xd9000000, fog ? 0x00230405 : 0x00220405);
        command(dl, 0xe3000a01, fog ? 0x00100000 : 0);
        command(dl, 0xe3001001, 0);
        command(dl, 0xe3001201, 0x00002000); // Filter enlarged terrain texels.
        // Fog consumes SHADE alpha. Keep brush coverage in primitive alpha;
        // cycle 1 modulates texel RGB by lighting, cycle 2 passes it through.
        command(dl, fog ? 0xfc1217ff : 0xfc127e24, fog ? 0xfffffe38 : 0xfffff9fc);
        command(dl, 0xd7000002, 0xffffffff);
        for (auto type = tiles.begin(); type != tiles.end(); ++type) {
            const FieldBrushShape shape(change.paint, type.key(), source.map.bounds,
                                        source.visualTerrain);
            for (int pass = 0; pass < (type->border.isNull() ? 1 : 2); ++pass) {
                const bool border = pass == 1;
                const auto &image = border ? type->border : type->texture;
                const auto painted = mesh(shape, type.value(), surface, border);
                const int w = image.width(), h = image.height(), line = (w * 2 + 7) / 8,
                          ms = int(std::log2(w)), mt = int(std::log2(h));
                const bool highway = type.key() == 1 || type.key() == 9;
                // Mirroring the continuous road body makes both ends of every
                // repeat meet at the same texels, including filtered samples.
                const quint32 cs = shape.road() ? 2 : 0, ct = shape.mountain() || border ? 2
                                                              : highway                  ? 1
                                                                                         : 0;
                const auto tex = out.add(textureBytes(image), 8);
                command(dl, 0xe7000000, 0);
                command(dl, 0xfd100000, tex);
                command(dl, 0xf5100000, 0x07000000);
                command(dl, 0xe6000000, 0);
                const int dxt = (2048 + line - 1) / line;
                command(dl, 0xf3000000, 0x07000000 | (quint32(w * h - 1) << 12) | quint32(dxt));
                command(dl, 0xe7000000, 0);
                command(dl, 0xf5100000 | (quint32(line) << 9),
                        (ct << 18) | (quint32(mt) << 14) | (cs << 8) | (quint32(ms) << 4));
                command(dl, 0xf2000000, (quint32((w - 1) * 4) << 12) | quint32((h - 1) * 4));
                // Reuse vertices within the RSP's 32-entry cache, and emit
                // two triangles per command. Storage, not stroke count, is the limit.
                for (bool translucent : {false, true}) {
                    command(dl, 0xe7000000, 0);
                    // Fog maps retain their original FOG_SHADE_A first blender;
                    // ordinary maps use AA_ZB_XLU_SURF / OPA_SURF in both cycles.
                    command(dl, 0xe200001c,
                            fog ? (translucent ? 0xc81049d8 : 0xc8112078)
                                : (translucent ? 0x005049d8 : 0x00552078));
                    QByteArray vertices;
                    QMap<QByteArray, int> cache;
                    QVector<std::array<int, 3>> faces;
                    auto indices = [](const std::array<int, 3> &f) {
                        return (quint32(f[0] * 2) << 16) | (quint32(f[1] * 2) << 8) |
                               quint32(f[2] * 2);
                    };
                    auto flush = [&] {
                        if (faces.isEmpty())
                            return;
                        const auto n = quint32(vertices.size() / 16);
                        command(dl, 0x01000000 | (n << 12) | (n << 1), out.add(vertices, 16));
                        for (int i = 0; i < faces.size(); i += 2) {
                            if (i + 1 < faces.size())
                                command(dl, 0x06000000 | indices(faces[i]), indices(faces[i + 1]));
                            else
                                command(dl, 0x05000000 | indices(faces[i]), 0);
                        }
                        vertices.clear();
                        faces.clear();
                        cache.clear();
                    };
                    int priorAlpha = -1;
                    for (const auto &points : painted) {
                        const bool blend =
                            points[0].alpha < 255 || points[1].alpha < 255 || points[2].alpha < 255;
                        if (blend != translucent)
                            continue;
                        if (fog) {
                            // Quantized per-face coverage keeps narrow feathered
                            // joins while allowing the RSP to write per-vertex fog.
                            const int alpha = std::clamp(
                                int(std::round(
                                    (points[0].alpha + points[1].alpha + points[2].alpha) / 51.)) *
                                    17,
                                0, 255);
                            if (alpha != priorAlpha) {
                                flush();
                                command(dl, 0xe7000000, 0);
                                command(dl, 0xfa000000, 0xffffff00u | quint32(alpha));
                                priorAlpha = alpha;
                            }
                        }
                        const auto mid = (points[0].uv + points[1].uv + points[2].uv) / 3;
                        const double os = cs & 2 ? 0 : std::floor(mid.x() / (w * 8)) * (w * 8),
                                     ot = ct & 2 ? 0 : std::floor(mid.y() / (h * 8)) * (h * 8);
                        std::array<QByteArray, 3> packed;
                        int missing = 0;
                        for (int v = 0; v < 3; ++v) {
                            auto &data = packed[v];
                            data = QByteArray(16, 0);
                            for (int axis = 0; axis < 3; ++axis) {
                                const double p = points[v].p[axis] * 64;
                                if (!std::isfinite(p) || std::abs(p) > 32767)
                                    throw Error("Paint geometry exceeds vertex range");
                                put16(data, axis * 2, quint16(qint16(std::round(p))));
                                put8(data, 12 + axis,
                                     quint8(qint8(std::round(points[v].normal[axis] * 127))));
                            }
                            put16(data, 8,
                                  quint16(qint16(std::round((points[v].uv.x() - os) * 32))));
                            put16(data, 10,
                                  quint16(qint16(std::round((points[v].uv.y() - ot) * 32))));
                            put8(data, 15, points[v].alpha);
                            missing += !cache.contains(data);
                        }
                        if (cache.size() + missing > 32)
                            flush();
                        std::array<int, 3> face;
                        for (int v = 0; v < 3; ++v) {
                            if (!cache.contains(packed[v])) {
                                cache[packed[v]] = int(vertices.size() / 16);
                                vertices += packed[v];
                            }
                            face[v] = cache[packed[v]];
                        }
                        faces << face;
                    }
                    flush();
                }
            }
        }
        command(dl, 0xdf000000, 0);
        word(program, 1);
        word(program, out.add(dl));
    }
    word(program, 0);
    const auto programAddress = out.add(program, 4);
    put32(out.bytes, 0, (programAddress & 0xffffff) + out.geometry);
    return out.bytes;
}
} // namespace lordly
