#pragma once
#include <QByteArray>
#include <QImage>
#include <QPoint>
#include <QVector>
#include <functional>
#include <optional>
class QWidget;
namespace lordly {
struct SpriteAsset;
struct PreparedSpriteSequence {
    QVector<QByteArray> sprites;
    QPoint origin;
};
std::optional<PreparedSpriteSequence> prepareSpriteSequence(QWidget *, const SpriteAsset &);
struct ArtImageMapping {
    QByteArray data;
    QImage image;
};
struct ArtImageAssignment {
    QSize target;
    QString objectPrefix, title, description;
    bool opaque = false;
    bool binaryAlpha = true;
    bool allowDither = false;
};
QImage loadArtworkImage(const QString &path);
std::optional<QByteArray> prepareArtworkImage(
    QWidget *, const QImage &, const ArtImageAssignment &,
    const std::function<ArtImageMapping(const QImage &, int alphaThreshold, bool orderedDither)> &);
} // namespace lordly
