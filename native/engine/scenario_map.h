#pragma once
#include "engine/image.h"
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <optional>

namespace lordly {
// World X/Z coordinates. The image is a separate reference: its projection is
// not assumed to equal the gameplay rectangle.
struct ScenarioMap {
    QRectF bounds;
    QVector<QPointF> points; // One-based placement selectors.
    QString imageName;
    QString placementNotice;
    std::optional<RasterImage> image;
    std::optional<QRectF> imageBounds; // Calibrated approximate world rectangle, source-bound.
    std::optional<QRectF> imageGridBounds() const;
    static ScenarioMap load(const Rom &rom, int key);
    QPointF world(int x, int z) const;
    QPoint bytes(QPointF position, bool selectorEncoding) const;
    std::optional<QPointF> placement(int x, int z) const;
};
} // namespace lordly
