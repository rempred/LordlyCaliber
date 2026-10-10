#pragma once
#include <QByteArray>
#include <QPointF>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <queue>
namespace lordly {
// Reconstruct a centerline from the saved paint mask, so old projects also get
// full-width road texels and lengthwise texture flow through curved strokes.
class FieldRoadUv {
    struct Sample {
        QPointF point, tangent;
        double length, radius;
    };
    QVector<Sample> samples_;

  public:
    FieldRoadUv(const QByteArray &region, int type, const QVector<float> &distance) {
        QByteArray line(65536, 0);
        for (int i = 0; i < line.size(); ++i)
            line[i] = quint8(region[i]) == type;
        const int offsets[] = {-256, -255, 1, 257, 256, 255, -1, -257};
        // Topology-preserving thinning: do not split bends or junctions.
        bool changed = true;
        while (changed) {
            changed = false;
            for (int pass = 0; pass < 2; ++pass) {
                QVector<int> remove;
                for (int y = 1; y < 255; ++y)
                    for (int x = 1; x < 255; ++x) {
                        const int at = y * 256 + x;
                        if (!line[at])
                            continue;
                        int n[8], count = 0, transitions = 0;
                        for (int k = 0; k < 8; ++k) {
                            n[k] = line[at + offsets[k]];
                            count += n[k];
                        }
                        for (int k = 0; k < 8; ++k)
                            transitions += !n[k] && n[(k + 1) % 8];
                        if (count < 2 || count > 6 || transitions != 1)
                            continue;
                        if (pass == 0 ? (n[0] * n[2] * n[4] || n[2] * n[4] * n[6])
                                      : (n[0] * n[2] * n[6] || n[0] * n[4] * n[6]))
                            continue;
                        remove << at;
                    }
                for (int at : remove)
                    line[at] = 0;
                changed |= !remove.isEmpty();
            }
        }
        QVector<double> along(65536, -1);
        using Node = std::pair<double, int>;
        for (int start = 0; start < line.size(); ++start) {
            if (!line[start] || along[start] >= 0)
                continue;
            std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;
            along[start] = 0;
            queue.push({0, start});
            while (!queue.empty()) {
                const auto [length, at] = queue.top();
                queue.pop();
                if (length > along[at])
                    continue;
                for (int k = 0; k < 8; ++k) {
                    const int next = at + offsets[k];
                    if (next < 0 || next >= 65536 || !line[next] ||
                        std::abs(next % 256 - at % 256) > 1)
                        continue;
                    const double value = length + (k & 1 ? std::sqrt(2.) : 1.);
                    if (along[next] < 0 || value < along[next]) {
                        along[next] = value;
                        queue.push({value, next});
                    }
                }
            }
        }
        for (int at = 0; at < line.size(); ++at)
            if (line[at]) {
                const int x = at % 256, y = at / 256;
                QPointF tangent;
                for (int dy = -3; dy <= 3; ++dy)
                    for (int dx = -3; dx <= 3; ++dx) {
                        if ((!dx && !dy) || x + dx < 0 || x + dx >= 256 || y + dy < 0 ||
                            y + dy >= 256)
                            continue;
                        const int next = (y + dy) * 256 + x + dx;
                        if (!line[next] || std::abs(along[next] - along[at]) > 5)
                            continue;
                        tangent +=
                            QPointF(dx, dy) * (along[next] - along[at]) / double(dx * dx + dy * dy);
                    }
                const double norm = std::hypot(tangent.x(), tangent.y());
                tangent = norm > .0001 ? tangent / norm : QPointF(0, 1);
                samples_ << Sample{QPointF(x + .5, y + .5), tangent, along[at],
                                   std::max(.5, double(distance[(y + 1) * 258 + x + 1]))};
            }
    }
    // S spans both native edges. T is distance along the road, in grid cells.
    QPointF at(QPointF p) const {
        const Sample *nearest = nullptr;
        double best = 1e20;
        for (const auto &s : samples_) {
            const auto delta = p - s.point;
            const double d = delta.x() * delta.x() + delta.y() * delta.y();
            if (d < best) {
                best = d;
                nearest = &s;
            }
        }
        if (!nearest)
            return {.5, 0};
        const auto d = p - nearest->point, t = nearest->tangent;
        return {std::clamp(.5 + (d.x() * t.y() - d.y() * t.x()) / (2 * nearest->radius), 0., 1.),
                nearest->length + d.x() * t.x() + d.y() * t.y()};
    }
};
} // namespace lordly
