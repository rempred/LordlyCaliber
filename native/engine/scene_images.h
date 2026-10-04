#pragma once
#include "engine/background_image.h"
#include "engine/director.h"
#include <QJsonObject>
namespace lordly {
struct DecodedImage {
    RasterImage composite;
    QPoint origin, reference;
    QVector<ImageLayer> layers;
    QVector<RasterImage> frames;
    QStringList limitations;
};
DecodedImage decodeRawImage(const QByteArray &bytes, const QJsonObject &layout);
struct SceneImage {
    QString id, name, container, kind;
    quint32 key = 0;
    QJsonObject layout;
    QStringList members;
};
struct SceneBackgroundRequest {
    int word = 0, selector = -1;
    QStringList images;
    QString status;
};
struct SceneBackgrounds {
    int mode = -1;
    QString modeEvidence;
    QVector<SceneBackgroundRequest> requests;
};
class SceneImages {
  public:
    static const QMap<QString, SceneImage> &catalog();
    static DecodedImage decode(const Rom &rom, const QString &id, bool compound = true);
    // Resource selectors are read from the current ROM, never substituted from metadata.
    static QStringList environment(const Rom &rom, int selector);
    static QStringList foreground(const Rom &rom, int selector);
    static QStringList group(const Rom &rom, int selector);
    static SceneBackgrounds backgrounds(const Rom &rom, const DirectorResource &scene,
                                        int modeOverride = -1);
};
} // namespace lordly
