#pragma once
#include <QPointF>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
namespace lordly {
// Find a short interior crest, favouring the broad middle of a mountain.
// The mask, not the straight line between summits, decides which cells connect.
inline QVector<QPointF> fieldRidgePath(QPointF start, QPointF end, const QVector<float> &distance) {
    constexpr int size = 258;
    auto index = [](QPointF p) {
        return (std::clamp(int(p.y()), 0, 255) + 1) * size + std::clamp(int(p.x()), 0, 255) + 1;
    };
    auto point = [](int i) { return QPointF(i % size - .5, i / size - .5); };
    auto inside = [&](QPointF p) {
        const double x = std::clamp(p.x() + .5, 0., 257.), z = std::clamp(p.y() + .5, 0., 257.);
        const int ix = std::min(256, int(x)), iz = std::min(256, int(z));
        const double fx = x - ix, fz = z - iz;
        return (1 - fz) *
                       ((1 - fx) * distance[iz * size + ix] + fx * distance[iz * size + ix + 1]) +
                   fz * ((1 - fx) * distance[(iz + 1) * size + ix] +
                         fx * distance[(iz + 1) * size + ix + 1]) >
               0;
    };
    auto clear = [&](QPointF a, QPointF b) {
        const int steps = std::max(1, int(std::ceil(std::hypot(b.x() - a.x(), b.y() - a.y()) * 4)));
        for (int i = 0; i <= steps; ++i)
            if (!inside(a + (b - a) * (double(i) / steps)))
                return false;
        return true;
    };
    const int first = index(start), last = index(end);
    QVector<double> costs(size * size, std::numeric_limits<double>::infinity());
    QVector<int> previous(size * size, -1);
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pending;
    costs[first] = 0;
    pending.push({0, first});
    while (!pending.empty()) {
        const auto [cost, at] = pending.top();
        pending.pop();
        if (cost != costs[at])
            continue;
        if (at == last)
            break;
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dz)
                    continue;
                const int next = at + dz * size + dx;
                if (next < 0 || next >= distance.size() || distance[next] <= 0 ||
                    (dx && dz && (distance[at + dx] <= 0 || distance[at + dz * size] <= 0)))
                    continue;
                const double width = std::max(.25, double(std::min(distance[at], distance[next]))),
                             nextCost =
                                 cost + (dx && dz ? std::sqrt(2.) : 1.) * (1 + 8 / (width * width));
                if (nextCost < costs[next]) {
                    costs[next] = nextCost;
                    previous[next] = at;
                    pending.push({nextCost, next});
                }
            }
    }
    if (!std::isfinite(costs[last]))
        return {};
    QVector<QPointF> path{end};
    for (int at = last; at != first; at = previous[at])
        if (at != last)
            path << point(at);
    path << start;
    std::reverse(path.begin(), path.end());
    // Remove grid stair steps while keeping the routed crest's bends and never
    // crossing an inlet/cut. This is a geometric simplification, not a new peak.
    QVector<QPointF> result{start};
    for (int from = 0; from + 1 < path.size();) {
        int to = from + 1;
        for (int candidate = from + 2; candidate < path.size(); ++candidate) {
            const auto delta = path[candidate] - path[from];
            const double length = std::hypot(delta.x(), delta.y());
            bool close = true;
            for (int i = from + 1; i < candidate && close; ++i) {
                const auto p = path[i] - path[from];
                close = std::abs(p.x() * delta.y() - p.y() * delta.x()) <= length * .65;
            }
            if (!close || !clear(path[from], path[candidate]))
                break;
            to = candidate;
        }
        result << path[to];
        from = to;
    }
    return result;
}
} // namespace lordly
