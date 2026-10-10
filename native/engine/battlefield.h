#pragma once
#include "engine/rom.h"
#include <QImage>
#include <QMap>
#include <QRectF>
#include <QStringList>
#include <QVector3D>
#include <QVector>
#include <array>
#include <memory>
namespace lordly {
struct FieldTriangle {
    std::array<QVector3D, 3> points;
    int group = -1;
    int owner = -1, face = -1, terrain = 0;
};
struct FieldBrush {
    struct Vertex {
        QVector3D point;
        QPointF uv;
    };
    QImage texture, border;
    QVector<std::array<Vertex, 3>> bridge;
    bool bridgeFiltered = false;
    double densityS = 16, densityT = 16;
};
// Actual field geometry and materials. Rendering is an orthographic daylight
// editing view, not an emulation of the game's camera, weather or RDP coverage.
class Battlefield {
  public:
    static Battlefield load(const Rom &, int scenario, const QVector<quint16> &siteFlags = {},
                            const QByteArray &model = {});
    QImage render(QRectF worldBounds, int pixels = 1536) const;
    int triangleCount() const;
    int buildingCount() const;
    bool terrainFog() const;
    QStringList notices() const;
    quint32 resourceKey() const;
    QByteArray modelData() const;
    QVector<FieldTriangle> surface() const;
    QVector<FieldTriangle> terrainFaces(const QByteArray &grid, QRectF bounds) const;
    QVector<FieldTriangle> triangles() const;
    QMap<int, QByteArray> drawTransforms() const;
    QMap<int, int> siteGroups() const; // one-based site -> verified native culling group
    QMap<int, QRectF> groups() const;
    FieldBrush terrainBrush(int terrain, const QByteArray &grid, QRectF bounds) const;
    QByteArray terrainGrid(const QByteArray &gameplay, QRectF bounds) const;

  private:
    struct Data;
    std::shared_ptr<Data> data_;
};
} // namespace lordly
