#pragma once
#include <QByteArray>
#include <QImage>
#include <functional>
#include <optional>
class QWidget;
namespace lordly {
struct ArtImageMapping {
    QByteArray data;
    QImage image;
};
struct ArtImageAssignment {
    QSize target;
    QString objectPrefix, title, description;
    bool opaque = false;
};
QImage loadArtworkImage(const QString &path);
std::optional<QByteArray>
prepareArtworkImage(QWidget *, const QImage &, const ArtImageAssignment &,
                    const std::function<ArtImageMapping(const QImage &, int alphaThreshold)> &);
} // namespace lordly
