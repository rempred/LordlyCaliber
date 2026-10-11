#pragma once
#include <QPointF>
#include <QSizeF>
#include <QVector>
#include <algorithm>
#include <cmath>
namespace lordly {
// Oriented footprint shared by a native bridge template and a painted span.
struct FieldSpan {
    QPointF center, along, across;
    double length = 0, width = 0;
    static double dot(QPointF a, QPointF b) {
        return a.x() * b.x() + a.y() * b.y();
    }
    FieldSpan(const QVector<QPointF> &points, QSizeF cell = {}) {
        if (points.isEmpty())
            return;
        for (auto p : points)
            center += p;
        center /= double(points.size());
        double xx = 0, xz = 0, zz = 0;
        for (auto p : points) {
            p -= center;
            xx += p.x() * p.x();
            xz += p.x() * p.y();
            zz += p.y() * p.y();
        }
        const double angle = .5 * std::atan2(2 * xz, xx - zz);
        along = {std::cos(angle), std::sin(angle)};
        across = {-along.y(), along.x()};
        double lo = 1e9, hi = -1e9, left = 1e9, right = -1e9;
        for (auto p : points) {
            const double a = dot(p - center, along), b = dot(p - center, across);
            lo = std::min(lo, a);
            hi = std::max(hi, a);
            left = std::min(left, b);
            right = std::max(right, b);
        }
        center += along * ((lo + hi) / 2) + across * ((left + right) / 2);
        length = hi - lo + std::abs(along.x()) * std::max(0., cell.width()) +
                 std::abs(along.y()) * std::max(0., cell.height());
        width = right - left + std::abs(across.x()) * std::max(0., cell.width()) +
                std::abs(across.y()) * std::max(0., cell.height());
    }
    QPointF normalized(QPointF p) const {
        return {dot(p - center, along) / length, dot(p - center, across) / width};
    }
    QPointF world(QPointF p) const {
        return center + along * (p.x() * length) + across * (p.y() * width);
    }
};
} // namespace lordly
