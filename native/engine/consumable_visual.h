#pragma once
#include "engine/rom.h"
#include <QImage>
#include <QJsonObject>
#include <QVector>
namespace lordly {
// Native menu effects use four 32x32 CI4 frames and one shared RGBA5551 palette.
// Other pieces in the same resource (icons/numbers) are deliberately excluded.
struct ConsumableVisual {
    QString name;
    QByteArray pixels, palette;
    QJsonObject json() const;
    static ConsumableVisual fromJson(const QJsonObject &);
    static ConsumableVisual retail(const Rom &, int item);
    static QByteArray retailPixels(const Rom &);
    static QByteArray retailPalettes(const Rom &);
    static ConsumableVisual fromImages(const QString &, const QVector<QImage> &, bool smooth);
    static QImage fit(const QImage &, bool smooth);
    QImage frame(int) const;
    void validate() const;
};
} // namespace lordly
