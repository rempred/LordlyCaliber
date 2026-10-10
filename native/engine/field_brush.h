#pragma once
#include "engine/map_edits.h"
#include <algorithm>
#include <cmath>
namespace lordly {
// A brush mask describes a region, not a rectangle of repeating texture.
// Distances provide a continuous boundary and a base-to-ridge coordinate.
class FieldBrushShape {
    static constexpr int size = 258;
    QVector<float> distance_, peaks_;
    QRectF bounds_;
    int type_;
    double cell_;
    static void transform(QVector<float> &d) {
        constexpr float diagonal = 1.41421356f;
        for (int y = 1; y < size - 1; ++y)
            for (int x = 1; x < size - 1; ++x) {
                const int i = y * size + x;
                d[i] = std::min({d[i], d[i - 1] + 1, d[i - size] + 1, d[i - size - 1] + diagonal,
                                 d[i - size + 1] + diagonal});
            }
        for (int y = size - 2; y > 0; --y)
            for (int x = size - 2; x > 0; --x) {
                const int i = y * size + x;
                d[i] = std::min({d[i], d[i + 1] + 1, d[i + size] + 1, d[i + size - 1] + diagonal,
                                 d[i + size + 1] + diagonal});
            }
    }

  public:
    QRect cells;
    FieldBrushShape(const QByteArray &paint, int type, QRectF bounds)
        : distance_(size * size), peaks_(size * size, 0), bounds_(bounds), type_(type),
          cell_(std::min(bounds.width(), bounds.height()) / 256) {
        QVector<float> outside(size * size, 1000);
        int x0 = 256, z0 = 256, x1 = -1, z1 = -1;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                const int i = y * size + x;
                const bool inside = x > 0 && y > 0 && x <= 256 && y <= 256 &&
                                    u8(paint, (y - 1) * 256 + x - 1) == type;
                distance_[i] = inside ? 1000 : 0;
                outside[i] = inside ? 0 : 1000;
                if (inside) {
                    x0 = std::min(x0, x - 1);
                    x1 = std::max(x1, x - 1);
                    z0 = std::min(z0, y - 1);
                    z1 = std::max(z1, y - 1);
                }
            }
        cells = QRect(QPoint(x0, z0), QPoint(x1, z1));
        transform(distance_);
        transform(outside);
        for (int i = 0; i < distance_.size(); ++i)
            distance_[i] = distance_[i] > 0 ? distance_[i] - .5f : .5f - outside[i];
        // Each separate mountain gets its own peak. A narrow island must not
        // lose its light summit because another brush stroke is much wider.
        for (int y = 1; y <= 256; ++y)
            for (int x = 1; x <= 256; ++x) {
                const int start = y * size + x;
                if (distance_[start] <= 0 || peaks_[start] != 0)
                    continue;
                QVector<int> queue{start};
                peaks_[start] = -1;
                float peak = 0;
                for (int n = 0; n < queue.size(); ++n) {
                    const int i = queue[n];
                    peak = std::max(peak, distance_[i]);
                    for (int j : {i - 1, i + 1, i - size, i + size})
                        if (j >= 0 && j < peaks_.size() && distance_[j] > 0 && peaks_[j] == 0) {
                            peaks_[j] = -1;
                            queue << j;
                        }
                }
                for (int i : queue)
                    peaks_[i] = peak;
            }
    }
    QPointF world(QPointF grid) const {
        return {bounds_.left() + grid.x() * bounds_.width() / 256,
                bounds_.top() + grid.y() * bounds_.height() / 256};
    }
    QPointF grid(QPointF world) const {
        return {(world.x() - bounds_.left()) * 256 / bounds_.width(),
                (world.y() - bounds_.top()) * 256 / bounds_.height()};
    }
    double distance(QPointF p) const {
        const double gx = std::clamp(p.x() + .5, 0., 257.), gy = std::clamp(p.y() + .5, 0., 257.);
        const int x = std::min(256, int(gx)), y = std::min(256, int(gy));
        const double fx = gx - x, fy = gy - y;
        return ((1 - fy) * ((1 - fx) * distance_[y * size + x] + fx * distance_[y * size + x + 1]) +
                fy * ((1 - fx) * distance_[(y + 1) * size + x] +
                      fx * distance_[(y + 1) * size + x + 1])) *
               cell_;
    }
    double peak(QPointF p) const {
        const int x = std::clamp(int(p.x() + .5), 0, 256), y = std::clamp(int(p.y() + .5), 0, 256);
        return std::max({peaks_[y * size + x], peaks_[y * size + x + 1], peaks_[(y + 1) * size + x],
                         peaks_[(y + 1) * size + x + 1], .5f}) *
               cell_;
    }
    bool mountain() const {
        return type_ == 7 || type_ == 14;
    }
    bool forest() const {
        return type_ == 5 || type_ == 13;
    }
    bool road() const {
        return type_ == 1 || type_ == 9 || type_ == 2 || type_ == 10;
    }
    double edgeWidth() const {
        return .5;
    }
    double rise(QPointF p) const {
        const double d = std::max(0., distance(p));
        if (mountain()) {
            const double max = peak(p);
            return std::min(3.2, max * .7) * std::pow(std::clamp(d / max, 0., 1.), .85);
        }
        if (forest())
            return .28 * std::min(1., d / edgeWidth()) +
                   .27 * (1 - std::exp(-std::max(0., d - edgeWidth())));
        return 0;
    }
};
} // namespace lordly
