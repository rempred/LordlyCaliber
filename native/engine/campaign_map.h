#pragma once
#include "engine/image.h"
#include "engine/story_routes.h"
#include <QJsonObject>
namespace lordly {
// Physical world-map picking and drawing both visit the first40 records.
// Later records are story aliases, not additional selectable map capacity.
class CampaignMap {
  public:
    static constexpr int nodeCount = 40, roadCount = 40;
    static constexpr int roadCapacity = 96, mapWidth = 512, mapHeight = 320;
    explicit CampaignMap(const Rom &);
    QPoint position(int node) const;
    QString name(int node) const;
    void setName(int location, const QString &);
    QMap<int,QString> relocatedNames() const;
    int label(int node) const;
    QMap<int, QString> labels() const {
        return labels_;
    }
    QPair<int, int> road(int id) const;
    int totalRoads() const {
        return roads_.size() / 2;
    }
    int addRoad(int from, int to);
    // Travel bends stay within the native waypoint array. Visible dots are
    // sampled along that same path, independently of its number of bends.
    QVector<QPoint> roadWaypoints(int id) const;
    QVector<QPoint> roadDots(int id) const;
    QVector<QPoint> pulledRoad(int id, int dot, QPoint delta) const;
    void setRoadDots(int id, const QVector<QPoint> &);
    void removeRoad(int id);
    QByteArray roadBytes() const {
        return roads_;
    }
    QByteArray dotBytes() const;
    RasterImage background() const;
    RasterImage nodeArtwork(int node) const;
    RasterImage iconArtwork(int icon) const;
    int icon(int node) const;
    void setIcon(int node,int icon);
    QByteArray iconBytes() const { return icons_; }
    RasterImage roadArtwork() const;
    QVector<int> availableNodes() const;
    void setNode(int id, QPoint position, int label);
    void setRoad(int id, int from, int to);
    QJsonObject json() const;
    void apply(const QJsonObject &);
    QByteArray rebuild(const QByteArray &) const;
    int changedNodes() const;

  private:
    QByteArray original_, nodes_, originalRoads_, roads_, originalDots_, worldEntry_;
    QByteArray mapArtwork_, markers_, originalIcons_, icons_;
    QMap<int, QString> labels_;
    QMap<int, QString> names_;
    QMap<quint32,QString> ownedNamePointers_;
    QMap<int, quint32> pointers_;
    struct RoadShape { QPoint from, to; QVector<QPoint> points; };
    QMap<int, RoadShape> shapes_;
    QVector<QVector<QPoint>> originalDisplayDots_;
};
} // namespace lordly
