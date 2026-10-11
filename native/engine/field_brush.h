#pragma once
#include "engine/field_mountain.h"
#include "engine/field_road_uv.h"
#include "engine/map_edits.h"
#include <algorithm>
#include <cmath>
namespace lordly {
// A brush mask describes a region, not a rectangle of repeating texture.
// Distances provide a continuous boundary and a base-to-ridge coordinate.
class FieldBrushShape {
    static constexpr int size = 258;
    QVector<float> distance_;
    QRectF bounds_;
    int type_;
    double cell_;
    std::shared_ptr<FieldBrushShape> original_, coverage_;
    std::shared_ptr<FieldRoadUv> road_;
    std::shared_ptr<FieldMountain> mountain_;
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
    FieldBrushShape(const QByteArray &paint, int type, QRectF bounds, const QByteArray &native = {},
                    const QVector<QVector3D> &summits = {})
        : distance_(size * size), bounds_(bounds), type_(type),
          cell_(std::min(bounds.width(), bounds.height()) / 256) {
        auto region = paint;
        if (!native.isEmpty()) {
            QByteArray changed(65536, char(-1));
            for (int i = 0; i < 65536; ++i) {
                if (u8(paint, i) == 255)
                    region[i] = native[i];
                if (u8(paint, i) == type && u8(native, i) != type)
                    changed[i] = char(type);
            }
            original_ =
                std::make_shared<FieldBrushShape>(native, type, bounds, QByteArray{}, summits);
            coverage_ =
                std::make_shared<FieldBrushShape>(changed, type, bounds, QByteArray{}, summits);
        }
        QVector<float> outside(size * size, 1000);
        int x0 = 256, z0 = 256, x1 = -1, z1 = -1;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                const int i = y * size + x;
                const bool inside = x > 0 && y > 0 && x <= 256 && y <= 256 &&
                                    u8(region, (y - 1) * 256 + x - 1) == type;
                distance_[i] = inside ? 1000 : 0;
                outside[i] = inside ? 0 : 1000;
                if (inside) {
                    x0 = std::min(x0, x - 1);
                    x1 = std::max(x1, x - 1);
                    z0 = std::min(z0, y - 1);
                    z1 = std::max(z1, y - 1);
                }
            }
        cells = x1 < x0 ? QRect() : QRect(QPoint(x0, z0), QPoint(x1, z1));
        transform(distance_);
        transform(outside);
        for (int i = 0; i < distance_.size(); ++i)
            distance_[i] = distance_[i] > 0 ? distance_[i] - .5f : .5f - outside[i];
        if (mountain() || forest()) {
            // Round the raster mask's cell corners before tracing the mesh.
            // Half-strength smoothing retains even a one-cell island; the
            // outline moves less than a cell, within the movement grid's scale.
            const auto raw = distance_;
            for (int y = 1; y <= 256; ++y)
                for (int x = 1; x <= 256; ++x) {
                    const int i = y * size + x;
                    const float blurred =
                        (raw[i] * 4 + (raw[i - 1] + raw[i + 1] + raw[i - size] + raw[i + size]) * 2 +
                         raw[i - size - 1] + raw[i - size + 1] + raw[i + size - 1] + raw[i + size + 1]) / 16;
                    distance_[i] = (raw[i] + blurred) * .5f;
                }
        }
        if (coverage_ && road())
            road_ = std::make_shared<FieldRoadUv>(region, type, distance_);
        if (mountain())
            mountain_ = std::make_shared<FieldMountain>(distance_, summits);
        if (coverage_) {
            const int band = int(std::ceil(.65 / cell_)) + 1;
            cells = coverage_->cells.isEmpty() ? QRect()
                                               : coverage_->cells.adjusted(-band, -band, band, band)
                                                     .intersected(QRect(0, 0, 256, 256));
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
    bool mountain() const {
        return type_ == 7 || type_ == 14;
    }
    bool forest() const {
        return type_ == 5 || type_ == 13;
    }
    bool road() const {
        return roadPaint(type_) || type_ == 2 || type_ == 10;
    }
    QPointF roadUv(QPointF p) const {
        auto uv = road_ ? road_->at(p) : QPointF(.5, p.y());
        uv.setY(uv.y() * cell_);
        return uv;
    }
    double edgeWidth() const {
        return .5;
    }
    QPointF mountainUv(QPointF p) const {
        return mountain_->sample(p, std::max(0., distance(p)) / cell_, cell_).uv;
    }
    double edgeOpacity(QPointF p) const {
        return std::clamp(distance(p) / (cell_ * .75), 0., 1.);
    }
    double profile(QPointF p) const {
        const double d = std::max(0., distance(p));
        if (mountain()) {
            return mountain_->sample(p, d / cell_, cell_).height;
        }
        if (forest())
            return .28 * std::min(1., d / edgeWidth()) +
                   .27 * (1 - std::exp(-std::max(0., d - edgeWidth())));
        return 0;
    }
    double influence(QPointF p) const {
        return coverage_ ? coverage_->distance(p) + .65 : distance(p);
    }
    double opacity(QPointF p) const {
        if (!coverage_)
            return 1;
        if (cells.isEmpty())
            return 0;
        const double d = coverage_->distance(p);
        if (d >= 0)
            return 1;
        return original_->distance(p) >= -.02 ? std::clamp(1 + d / .65, 0., 1.) : 0.;
    }
    double rise(QPointF p) const {
        if (!original_)
            return profile(p);
        return std::max(0., profile(p) - original_->profile(p)) * opacity(p);
    }
};
} // namespace lordly
