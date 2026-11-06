#pragma once
#include "engine/image.h"
#include "engine/story_routes.h"
#include <QJsonObject>
#include <QImage>
namespace lordly {
// Physical world-map picking and drawing both visit the first40 records.
// Later records are story aliases, not additional selectable map capacity.
class CampaignMap {
  public:
    static constexpr int nodeCount = 40, roadCount = 40;
    static constexpr int roadCapacity = 96, bendCapacity = 14, mapWidth = 512, mapHeight = 320;
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
    // A connection may use two sections of seven bends. Runtime travel pages
    // them through the native buffer; visible dots sample the full path.
    QVector<QPoint> roadWaypoints(int id) const;
    QVector<QPoint> roadDots(int id) const;
    // Drawing only: preserve road and dot indices for independent picking/editing.
    static QVector<QVector<QPoint>> mergedRoadDots(const QVector<QVector<QPoint>> &);
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
    static constexpr int iconCapacity = 64;
    QList<int> iconIds() const { return artworkIcons_.keys(); }
    QString iconName(int) const;
    int iconKind(int) const;
    QByteArray iconFrameBytes(int, int state, int frame) const;
    QImage iconFrame(int, int state, int frame) const;
    void setIconFrame(int, int state, int frame, const QByteArray &);
    void renameIcon(int, const QString &);
    int createIcon(const QString &, int copyFrom);
    void removeIcon(int);
    QJsonObject iconCatalog() const;
    void loadIconCatalog(const QJsonObject &);
    static QByteArray prepareArtwork(const QImage &, int colors, bool opaque, bool dither = false);
    static QImage artworkImage(const QByteArray &, QSize, int colors);
    const QByteArray &backgroundBytes() const { return mapArtwork_; }
    void setBackground(const QByteArray &);
    void restoreBackground() { mapArtwork_ = originalMapArtwork_; }
    QJsonObject artworkEdits() const;
    int icon(int node) const;
    void setIcon(int node,int icon);
    QByteArray iconBytes() const;
    QByteArray iconAssignments() const { return icons_; }
    RasterImage roadArtwork() const;
    QVector<int> availableNodes(bool clearedStory = false) const;
    void setNode(int id, QPoint position, int label);
    void startEmpty(int startingLocation);
    void setRoad(int id, int from, int to);
    QJsonObject json() const;
    void apply(const QJsonObject &);
    QByteArray rebuild(const QByteArray &) const;
    int changedNodes() const;

  private:
    QByteArray original_, nodes_, originalRoads_, roads_, originalDots_, worldEntry_;
    QByteArray mapArtwork_, markers_, originalIcons_, icons_;
    struct IconArt { QString name; int kind = 0; QByteArray data; };
    QMap<int, IconArt> artworkIcons_, originalArtworkIcons_;
    QByteArray originalMapArtwork_;
    void initializeArtwork();
    QMap<int, QString> labels_;
    QMap<int, QString> names_;
    QMap<quint32,QString> ownedNamePointers_;
    QMap<int, quint32> pointers_;
    struct RoadShape { QPoint from, to; QVector<QPoint> points; };
    QMap<int, RoadShape> shapes_;
    QVector<QVector<QPoint>> originalDisplayDots_;
    int originalBendCapacity_ = 7;
};
} // namespace lordly
