#pragma once
#include "engine/image.h"
#include <QPoint>
#include <QStringList>
#include <QVector>
namespace lordly {
struct ImageLayer {
    QPoint origin;
    RasterImage image;
};
struct BackgroundImage {
    int format = 0, declaredLayers = 0;
    QPoint reference, origin;
    QVector<ImageLayer> layers;
    RasterImage composite;
    QStringList limitations;
};
RasterImage decodeKImage(const QByteArray &source);
// HUFF uses the native coefficient order and scale with a floating-point IDCT.
// Exact RSP fixed-point rounding is not claimed.
RasterImage decodeHuffImage(const QByteArray &source, bool embedded);
BackgroundImage decodeBackgroundImage(const QByteArray &source);
} // namespace lordly
