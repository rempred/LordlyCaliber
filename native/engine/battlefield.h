#pragma once
#include "engine/rom.h"
#include <QImage>
#include <QRectF>
#include <QStringList>
#include <QVector>
#include <memory>
namespace lordly {
// Actual field geometry and materials. Rendering is an orthographic daylight
// editing view, not an emulation of the game's camera, weather or RDP coverage.
class Battlefield {
  public:
    static Battlefield load(const Rom &, int scenario, const QVector<quint16> &siteFlags = {});
    QImage render(QRectF worldBounds, int pixels = 1536) const;
    int triangleCount() const;
    int buildingCount() const;
    QStringList notices() const;
    quint32 resourceKey() const;

  private:
    struct Data;
    std::shared_ptr<Data> data_;
};
} // namespace lordly
