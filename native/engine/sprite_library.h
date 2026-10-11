#pragma once
#include "engine/rom.h"
#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QVector>
namespace lordly {
struct SpriteLayer {
    bool operator==(const SpriteLayer &) const = default;
    QString id, name;
    bool visible = true;
    QPoint position;
    QImage image;
};
struct SpriteAssetFrame {
    bool operator==(const SpriteAssetFrame &) const = default;
    QString id, name;
    int ticks = 8;
    QVector<SpriteLayer> layers;
};
struct SpriteAsset {
    bool operator==(const SpriteAsset &) const = default;
    QString id, name;
    QString kind = "sprite";
    QSize size;
    QPoint anchor;
    QVector<SpriteAssetFrame> frames;
    QJsonObject provenance;
    static SpriteAsset blank(const QString &name, QSize);
    static SpriteAsset fromImage(const QString &name, const QImage &);
    static SpriteAsset fromJson(const QJsonObject &, int version);
    static SpriteAsset fromAssetFile(const QJsonObject &);
    QJsonObject json(bool browserCompatible = false) const;
    QJsonObject assetFile(bool browserCompatible = false) const;
    QString effectiveKind() const;
    quint64 pixels() const;
    void validate() const;
    QImage composite(int frame) const;
    SpriteAsset resized(QSize, bool scaleLayers, QPoint canvasShift = {}, bool crop = false) const;
    SpriteAsset rotated(bool clockwise) const;
};
class SpriteLibrary {
  public:
    static constexpr quint64 pixelLimit = 64ULL * 1024 * 1024;
    static constexpr int dimensionLimit = 4096;
    static QString newId();
    static SpriteLibrary fromJson(const QJsonObject &);
    QJsonObject json(bool browserCompatible = false) const;
    const QVector<SpriteAsset> &assets() const {
        return assets_;
    }
    const SpriteAsset &asset(const QString &) const;
    void replace(const SpriteAsset &);
    void remove(const QString &);
    void merge(const SpriteLibrary &);

  private:
    QVector<SpriteAsset> assets_;
};
QImage spriteLine(QImage, QPoint from, QPoint to, const QColor &, int brush = 1,
                  QRect selection = {});
QImage spriteFill(QImage, QPoint start, const QColor &, QRect selection = {});
QImage spriteShift(const QImage &, QPoint offset, bool wrap = false);
QImage spriteScale(const QImage &, QSize);
QImage spritePlaceNearest(const QImage &, QSize canvas, QRectF placement);
QImage spriteMoveSelection(const QImage &, QRect, QPoint);
} // namespace lordly
