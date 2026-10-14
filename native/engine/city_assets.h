#pragma once
#include <QColor>
#include <QImage>
#include <QRectF>
#include <QVector3D>
#include <QVector>
#include <array>
namespace lordly {
struct CityMaterial {
    QImage texture;
    int cs = 2, ct = 2, ms = 0, mt = 0;
    bool filtered = false, lit = false;
};
struct CityVertex {
    QVector3D point, normal;
    QPointF uv;
    QColor color;
};
struct CityFace {
    std::array<CityVertex, 3> vertices;
    int material = 0;
};
struct CityAsset {
    QString id, name;
    int scenario = 0;
    QVector<int> groups;
    QVector<CityFace> faces;
    QVector<CityMaterial> materials;
    QRectF bounds;
    QImage thumbnail;
};
struct CityPlacement {
    QString asset;
    QPointF position; // World X/Z, independent of the fine terrain raster.
    int rotation = 0;
    bool operator==(const CityPlacement &) const = default;
};
inline bool cityPaint(int type) {
    return type >= 32 && type <= 35;
}
inline int fieldPaintTerrain(int type) {
    return cityPaint(type) ? 1 : type;
}
inline bool roadPaint(int type) {
    return type == 1 || type == 9 || (type >= 33 && type <= 35);
}
} // namespace lordly
