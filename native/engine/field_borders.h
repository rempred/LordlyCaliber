#pragma once
#include "engine/map_edits.h"
#include <QSet>
#include <algorithm>
#include <cmath>
namespace lordly {
// Rebuild an original raised region when any brush changes its outline. Unchanged
// regions retain their original faces; all drawing state remains in the source.
struct FieldBorders {
    FieldChanges change;
    QByteArray native, replaced = QByteArray(65536, 0);
    QMap<int, QSet<int>> removed;
    static bool raised(int type) {
        return type == 5 || type == 7 || type == 13 || type == 14;
    }
    bool removes(const FieldTriangle &t) const {
        const auto it = removed.constFind(t.owner);
        return it != removed.cend() && it->contains(t.face);
    }
    FieldBorders(const FieldMapSource &s, const FieldChanges &c)
        : change(c), native(s.visualTerrain) {
        QSet<int> heightCells;
        for (const auto &p : c.peaks) {
            const int at = int(p.position.y()) * 256 + int(p.position.x());
            if (at >= 0 && at < 65536 && u8(s.visualTerrain, at) == p.terrain)
                heightCells.insert(at);
        }
        for (int start = 0; start < 65536; ++start) {
            const int old = u8(s.visualTerrain, start);
            if (!raised(old) || replaced[start])
                continue;
            bool touched = heightCells.contains(start);
            for (int y = std::max(0, start / 256 - 1); y <= std::min(255, start / 256 + 1); ++y)
                for (int x = std::max(0, start % 256 - 1); x <= std::min(255, start % 256 + 1);
                     ++x) {
                    const int at = y * 256 + x, next = u8(c.paint, at);
                    touched |= next != 255 && next != u8(s.visualTerrain, at);
                }
            if (!touched)
                continue;
            QVector<int> queue{start};
            replaced[start] = char(old);
            for (int n = 0; n < queue.size(); ++n) {
                const int at = queue[n];
                for (int offset : {-1, 1, -256, 256}) {
                    const int to = at + offset;
                    if (to < 0 || to >= 65536 || std::abs(to % 256 - at % 256) > 1 ||
                        replaced[to] || u8(s.visualTerrain, to) != old)
                        continue;
                    replaced[to] = char(old);
                    queue << to;
                }
            }
        }
        if (replaced == QByteArray(65536, 0))
            return;
        for (int i = 0; i < 65536; ++i)
            if (replaced[i]) {
                if (u8(change.paint, i) == 255)
                    change.paint[i] = replaced[i];
                native[i] = 0;
            }
        const auto b = s.map.bounds;
        auto grid = [&](QVector3D p) {
            return QPointF((p.x() - b.left()) * 256 / b.width(),
                           (p.z() - b.top()) * 256 / b.height());
        };
        auto touches = [&](QPointF p, int type) {
            for (int y = int(std::floor(p.y())) - 1; y <= int(std::floor(p.y())) + 1; ++y)
                for (int x = int(std::floor(p.x())) - 1; x <= int(std::floor(p.x())) + 1; ++x)
                    if (x >= 0 && x < 256 && y >= 0 && y < 256 && u8(replaced, y * 256 + x) == type)
                        return true;
            return false;
        };
        for (const auto &t : s.terrainFaces) {
            if (t.owner < 0 || !raised(t.terrain))
                continue;
            const QPointF p[] = {grid(t.points[0]), grid(t.points[1]), grid(t.points[2])};
            bool hit = touches((p[0] + p[1] + p[2]) / 3, t.terrain);
            for (int i = 0; i < 3 && !hit; ++i)
                hit = touches(p[i], t.terrain) || touches((p[i] + p[(i + 1) % 3]) / 2, t.terrain);
            if (!hit) {
                const auto a = p[1] - p[0], d = p[2] - p[0];
                const double det = a.x() * d.y() - a.y() * d.x();
                if (std::abs(det) > 1e-8)
                    for (int y =
                             std::max(0, int(std::floor(std::min({p[0].y(), p[1].y(), p[2].y()}))));
                         y <= std::min(255,
                                       int(std::ceil(std::max({p[0].y(), p[1].y(), p[2].y()})))) &&
                         !hit;
                         ++y)
                        for (int x = std::max(
                                 0, int(std::floor(std::min({p[0].x(), p[1].x(), p[2].x()}))));
                             x <= std::min(255, int(std::ceil(
                                                    std::max({p[0].x(), p[1].x(), p[2].x()}))));
                             ++x) {
                            if (u8(replaced, y * 256 + x) != t.terrain)
                                continue;
                            const auto q = QPointF(x + .5, y + .5) - p[0];
                            const double u = (q.x() * d.y() - q.y() * d.x()) / det,
                                         v = (a.x() * q.y() - a.y() * q.x()) / det;
                            if (u >= 0 && v >= 0 && u + v <= 1) {
                                hit = true;
                                break;
                            }
                        }
            }
            if (hit)
                removed[t.owner].insert(t.face);
        }
    }
};
} // namespace lordly
