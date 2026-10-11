#pragma once
#include "engine/field_ridge.h"
#include <QPointF>
#include <QVector3D>
#include <QVector>
#include <algorithm>
#include <cmath>
namespace lordly {
// Slope panels run from a contour toward local ridge summits, rather than
// projecting one diagonal stripe across every side of a landform.
class FieldMountain {
    struct Peak {
        QPointF point;
        double radius, height = -1;
    };
    QVector<QVector<Peak>> regions_;
    struct Ridge {
        QVector<QPointF> points;
        double startHeight, endHeight, length = 0;
    };
    QVector<QVector<Ridge>> ridges_;
    QVector<int> labels_ = QVector<int>(258 * 258, -1);

  public:
    explicit FieldMountain(const QVector<float> &distance, const QVector<QVector3D> &hints = {}) {
        for (int y = 1; y <= 256; ++y)
            for (int x = 1; x <= 256; ++x) {
                const int start = y * 258 + x;
                if (distance[start] <= 0 || labels_[start] >= 0)
                    continue;
                const int region = int(regions_.size());
                QVector<int> cells{start}, candidates;
                labels_[start] = region;
                for (int n = 0; n < cells.size(); ++n) {
                    const int at = cells[n];
                    bool summit = true;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            summit &= distance[at + dy * 258 + dx] <= distance[at];
                    if (summit)
                        candidates << at;
                    for (int next : {at - 1, at + 1, at - 258, at + 258})
                        if (next >= 0 && next < labels_.size() && distance[next] > 0 &&
                            labels_[next] < 0) {
                            labels_[next] = region;
                            cells << next;
                        }
                }
                std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
                    return distance[a] == distance[b] ? a < b : distance[a] > distance[b];
                });
                QVector<Peak> peaks;
                // Surviving authored summits take precedence over mask maxima.
                for (const auto &hint : hints) {
                    if (hint.x() < 0 || hint.x() >= 256 || hint.z() < 0 || hint.z() >= 256)
                        continue;
                    const int hx = int(hint.x()) + 1, hy = int(hint.z()) + 1, at = hy * 258 + hx;
                    if (labels_[at] == region && distance[at] > 1)
                        peaks << Peak{{hint.x(), hint.z()}, distance[at], hint.y()};
                }
                for (int at : candidates) {
                    // Avoid inventing sharp peaks in narrow foothills. New peaks
                    // around an authored summit require a substantial extension.
                    if (!candidates.isEmpty() && distance[at] < distance[candidates[0]] * .45)
                        continue;
                    const Peak p{{double(at % 258) - .5, double(at / 258) - .5}, distance[at]};
                    bool separate = true;
                    for (const auto &q : peaks)
                        separate &=
                            std::hypot(p.point.x() - q.point.x(), p.point.y() - q.point.y()) >
                            std::max(5., q.height >= 0 ? std::max(p.radius, q.radius) * 4
                                                       : std::min(p.radius, q.radius) * 1.8);
                    if (separate)
                        peaks << p;
                }
                regions_ << peaks;
            }
    }
    struct Sample {
        QPointF uv;
        double height = 0;
    };
    QVector<QVector3D> peaks(double cell) const {
        QVector<QVector3D> result;
        for (const auto &region : regions_)
            for (const auto &p : region)
                result << QVector3D(float(p.point.x()),
                                    float(p.height >= 0 ? p.height : std::min(3.2, p.radius * cell * .7)),
                                    float(p.point.y()));
        return result;
    }
    void remove(QPointF position) {
        for (auto &region : regions_)
            region.removeIf([&](const Peak &p) { return p.point == position; });
    }
    void add(QPointF position, double height) {
        if (position.x() < 0 || position.x() >= 256 || position.y() < 0 || position.y() >= 256)
            return;
        const int label = labels_[(int(position.y()) + 1) * 258 + int(position.x()) + 1];
        if (label >= 0)
            regions_[label] << Peak{position, 0, height};
    }
    void connectRidges(const QVector<float> &distance, double cell) {
        ridges_ = QVector<QVector<Ridge>>(regions_.size());
        for (int region = 0; region < regions_.size(); ++region) {
            auto peaks = regions_[region];
            std::sort(peaks.begin(), peaks.end(), [](const Peak &a, const Peak &b) {
                return a.point.x() == b.point.x() ? a.point.y() < b.point.y()
                                                  : a.point.x() < b.point.x();
            });
            struct Edge {
                int a, b;
                double length;
            };
            QVector<Edge> edges;
            QVector<int> parents;
            for (int a = 0; a < peaks.size(); ++a) {
                parents << a;
                for (int b = a + 1; b < peaks.size(); ++b) {
                    const auto d = peaks[b].point - peaks[a].point;
                    edges << Edge{a, b, std::hypot(d.x(), d.y())};
                }
            }
            std::stable_sort(edges.begin(), edges.end(),
                             [](auto a, auto b) { return a.length < b.length; });
            auto root = [&](int a) {
                while (parents[a] != a)
                    a = parents[a];
                return a;
            };
            auto height = [&](const Peak &p) {
                return p.height >= 0 ? p.height : std::min(3.2, p.radius * cell * .7);
            };
            for (const auto &edge : edges) {
                const int a = root(edge.a), b = root(edge.b);
                if (a == b)
                    continue;
                auto path = fieldRidgePath(peaks[edge.a].point, peaks[edge.b].point, distance);
                if (path.size() < 2)
                    continue;
                parents[b] = a;
                Ridge ridge{path, height(peaks[edge.a]), height(peaks[edge.b])};
                for (int i = 1; i < path.size(); ++i)
                    ridge.length +=
                        std::hypot(path[i].x() - path[i - 1].x(), path[i].y() - path[i - 1].y());
                if (ridge.length > 1e-6)
                    ridges_[region] << ridge;
            }
        }
    }
    QVector<QPointF> ridgeVertices() const {
        QVector<QPointF> result;
        for (const auto &region : ridges_)
            for (const auto &ridge : region) {
                for (int i = 1; i < ridge.points.size(); ++i) {
                    const auto a = ridge.points[i - 1], d = ridge.points[i] - a;
                    const int count = std::max(1, int(std::ceil(std::hypot(d.x(), d.y()) / 2)));
                    for (int n = 0; n < count; ++n)
                        result << a + d * (double(n) / count);
                }
                result << ridge.points.back();
            }
        return result;
    }
    // Blend the directions toward adjacent summits before assigning the slope
    // panels, avoiding unrelated angular seams through the middle of a ridge.
    Sample sample(QPointF p, double distance, double cell) const {
        const int x = std::clamp(int(std::floor(p.x())) + 1, 1, 256),
                  y = std::clamp(int(std::floor(p.y())) + 1, 1, 256);
        int label = labels_[y * 258 + x];
        if (label < 0)
            for (int dy = -1; dy <= 1 && label < 0; ++dy)
                for (int dx = -1; dx <= 1 && label < 0; ++dx)
                    label = labels_[(y + dy) * 258 + x + dx];
        if (label < 0 || regions_[label].isEmpty())
            return {};
        if (label < ridges_.size() && !ridges_[label].isEmpty()) {
            double total = 0, u = 0, profile = 0, height = 0;
            for (const auto &ridge : ridges_[label]) {
                double nearest = 1e99, along = 0, offset = 0, textureAlong = 0;
                for (int i = 1; i < ridge.points.size(); ++i) {
                    const auto a = ridge.points[i - 1], d = ridge.points[i] - a, q = p - a;
                    const double squared = QPointF::dotProduct(d, d), length = std::sqrt(squared),
                                 raw = QPointF::dotProduct(q, d) / squared,
                                 t = std::clamp(raw, 0., 1.);
                    const auto v = q - d * t;
                    const double r = QPointF::dotProduct(v, v);
                    if (r < nearest) {
                        nearest = r;
                        along = offset + length * t;
                        textureAlong = along;
                        // Turn the slope panels around rounded ends. Extending
                        // the straight strip through a cap makes parallel rings.
                        if ((i == 1 && raw < 0) || (i + 1 == ridge.points.size() && raw > 1)) {
                            const double outward = length * (raw < 0 ? -raw : raw - 1),
                                         across = std::abs(q.x() * d.y() - q.y() * d.x()) / length;
                            textureAlong += (raw < 0 ? -1 : 1) * std::atan2(outward, across) * 16 /
                                            3.141592653589793;
                        }
                    }
                    offset += length;
                }
                const double r = std::sqrt(nearest),
                             t = distance <= 0 ? 0 : distance / (distance + r),
                             f = along / ridge.length,
                             crest = (ridge.startHeight * (1 - f) + ridge.endHeight * f) *
                                     (1 - .8 * f * (1 - f)),
                             weight = 1 / std::pow(.25 + nearest, 2);
                total += weight;
                u += textureAlong / 4 * weight;
                // A low point between high summits is a saddle, not another
                // bright summit. Native connecting vertices use lower texture
                // rows as their height falls below the adjacent summit.
                const double reference = std::max(ridge.startHeight, ridge.endHeight);
                profile += (reference > 1e-6 ? crest * t / reference : 0) * weight;
                height += crest * t * weight;
            }
            return {{u / total, profile / total}, height / total};
        }
        double total = 0, s = 0, c = 0, profile = 0, height = 0;
        for (const auto &peak : regions_[label]) {
            const auto d = p - peak.point;
            const double r = std::hypot(d.x(), d.y());
            const double weight = 1 / std::pow(.25 + r * r, 2);
            const double t = distance <= 0 ? 0 : std::clamp(distance / (distance + r), 0., 1.);
            s += d.y() / std::max(.01, r) * weight;
            c += d.x() / std::max(.01, r) * weight;
            profile += t * weight;
            height += (peak.height >= 0 ? peak.height : std::min(3.2, peak.radius * cell * .7)) *
                      t * weight;
            total += weight;
        }
        return {{std::atan2(s, c) * 4 / 3.141592653589793, profile / total}, height / total};
    }
};
} // namespace lordly
