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
            throw Error("The painted map exceeds the 512 KiB geometry budget; simplify or erase "
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
struct Region {
    QRect cells;
    int type;
};
QVector<Region> regions(const QByteArray &paint) {
    QVector<Region> out;
    QMap<QPair<int, int>, int> previous;
    for (int y = 0; y < 256; ++y) {
        QMap<QPair<int, int>, int> row;
        for (int x = 0; x < 256;) {
            const int type = u8(paint, y * 256 + x);
            if (type == 255) {
                ++x;
                continue;
            }
            const int left = x;
            while (x < 256 && u8(paint, y * 256 + x) == type)
                ++x;
            const auto span = qMakePair(left, x);
            int index = previous.value(span, -1);
            if (index >= 0 && out[index].type == type)
                out[index].cells.setBottom(y);
            else {
                index = int(out.size());
                out << Region{QRect(left, y, x - left, 1), type};
            }
            row[span] = index;
        }
        previous = row;
    }
    return out;
}
QVector<QVector3D> clip(QVector<QVector3D> points, int axis, double edge, bool greater) {
    QVector<QVector3D> out;
    if (points.isEmpty())
        return out;
    auto prior = points.last();
    bool was = greater ? prior[axis] >= edge : prior[axis] <= edge;
    for (const auto &p : points) {
        const bool inside = greater ? p[axis] >= edge : p[axis] <= edge;
        if (inside != was) {
            const double f = (edge - prior[axis]) / (p[axis] - prior[axis]);
            out << prior + (p - prior) * float(f);
        }
        if (inside)
            out << p;
        prior = p;
        was = inside;
    }
    return out;
}
QByteArray textureBytes(const QImage &image) {
    if (image.size() != QSize(32, 32))
        throw Error("Invalid map brush texture");
    QByteArray out(2048, 0);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const auto c = image.pixelColor(x, y);
            put16(out, (y * 32 + x) * 2,
                  quint16((c.red() * 31 / 255 << 11) | (c.green() * 31 / 255 << 6) |
                          (c.blue() * 31 / 255 << 1) | 1));
        }
    return out;
}
} // namespace
QByteArray editFieldModel(const FieldMapSource &source, const FieldChanges &change,
                          const QMap<int, QImage> &tiles) {
    const auto &original = source.geometry[0].data;
    Append out{original, int((u32(original, 48) + 15) & ~15u)};
    const auto groups = source.field.siteGroups();
    QMap<int, QVector3D> movement;
    for (auto it = change.sites.begin(); it != change.sites.end(); ++it) {
        if (!groups.contains(it.key() + 1))
            throw Error("Town has no verified drawing group");
        const auto from = source.map.points[it.key()];
        const auto delta = it.value() - from;
        movement[groups[it.key() + 1]] = {
            float(delta.x()), float(heightAt(source, it.value()) - heightAt(source, from)),
            float(delta.y())};
    }
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
        const auto surfaces = source.field.surface();
        const auto areas = regions(change.paint);
        QMap<int, QVector<std::array<QVector3D, 3>>> painted;
        int count = 0;
        for (const auto &area : areas) {
            const auto &b = source.map.bounds;
            const double x0 = b.left() + area.cells.left() * b.width() / 256,
                         x1 = b.left() + (area.cells.right() + 1) * b.width() / 256,
                         z0 = b.top() + area.cells.top() * b.height() / 256,
                         z1 = b.top() + (area.cells.bottom() + 1) * b.height() / 256;
            for (const auto &t : surfaces) {
                QVector<QVector3D> poly{t.points[0], t.points[1], t.points[2]};
                poly = clip(poly, 0, x0, true);
                poly = clip(poly, 0, x1, false);
                poly = clip(poly, 2, z0, true);
                poly = clip(poly, 2, z1, false);
                for (int n = 1; n + 1 < poly.size(); ++n) {
                    const auto cross =
                        QVector3D::crossProduct(poly[n] - poly[0], poly[n + 1] - poly[0]);
                    if (cross.y() < .000001)
                        continue;
                    if (++count > 5000)
                        throw Error("This painting requires too many map triangles; simplify or "
                                    "erase some strokes");
                    painted[area.type] << std::array<QVector3D, 3>{poly[0], poly[n], poly[n + 1]};
                }
            }
        }
        QByteArray matrix(64, 0);
        for (int n = 0; n < 3; ++n)
            putFixed(matrix, n * 5, 1. / 64);
        putFixed(matrix, 15, 1);
        const auto matrixAddress = out.add(matrix, 16);
        QByteArray dl;
        command(dl, 0xe7000000, 0);
        command(dl, 0xda380003, matrixAddress);
        command(dl, 0xd9000000, 0x00200405);
        command(dl, 0xe3000a01, 0);
        command(dl, 0xe3001001, 0);
        command(dl, 0xe200001c, 0x00552078);
        command(dl, 0xfc127e24, 0xfffff9fc);
        command(dl, 0xd7000002, 0xffffffff);
        for (auto type = painted.begin(); type != painted.end(); ++type) {
            if (!tiles.contains(type.key()))
                throw Error("Missing map brush texture");
            const auto tex = out.add(textureBytes(tiles[type.key()]), 8);
            command(dl, 0xe7000000, 0);
            command(dl, 0xfd100000, tex);
            command(dl, 0xf5100000, 0x07000000);
            command(dl, 0xe6000000, 0);
            command(dl, 0xf3000000, 0x073ff100);
            command(dl, 0xe7000000, 0);
            command(dl, 0xf5101000, 0x00014050);
            command(dl, 0xf2000000, 0x0007c07c);
            for (int start = 0; start < type->size(); start += 10) {
                const int n = std::min(10, int(type->size()) - start), vertices = n * 3;
                QByteArray data(vertices * 16, 0);
                for (int tri = 0; tri < n; ++tri) {
                    const auto &points = (*type)[start + tri];
                    const auto mid = (points[0] + points[1] + points[2]) / 3;
                    const double ox = std::floor(mid.x() / 4) * 4, oz = std::floor(mid.z() / 4) * 4;
                    for (int v = 0; v < 3; ++v) {
                        const int at = (tri * 3 + v) * 16;
                        for (int axis = 0; axis < 3; ++axis) {
                            const double p = (points[v][axis] + (axis == 1 ? .03125 : 0)) * 64;
                            if (!std::isfinite(p) || std::abs(p) > 32767)
                                throw Error("Paint geometry exceeds vertex range");
                            put16(data, at + axis * 2, quint16(qint16(std::round(p))));
                        }
                        put16(data, at + 8,
                              quint16(qint16(std::round((points[v].x() - ox) * 256))));
                        put16(data, at + 10,
                              quint16(qint16(std::round((points[v].z() - oz) * 256))));
                        for (int c = 0; c < 4; ++c)
                            put8(data, at + 12 + c, 255);
                    }
                }
                const auto vertexAddress = out.add(data, 16);
                command(dl, 0x01000000u | (quint32(vertices) << 12) | (quint32(vertices) << 1),
                        vertexAddress);
                for (int tri = 0; tri < n; ++tri) {
                    const int v = tri * 3;
                    command(dl,
                            0x05000000u | (quint32(v * 2) << 16) | (quint32((v + 1) * 2) << 8) |
                                quint32((v + 2) * 2),
                            0);
                }
            }
        }
        command(dl, 0xdf000000, 0);
        const auto list = out.add(dl);
        word(program, 1);
        word(program, list);
    }
    word(program, 0);
    const auto programAddress = out.add(program, 4);
    put32(out.bytes, 0, (programAddress & 0xffffff) + out.geometry);
    return out.bytes;
}
} // namespace lordly
