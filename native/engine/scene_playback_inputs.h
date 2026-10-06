#pragma once
#include "engine/scene_playback.h"
#include <QJsonObject>
namespace lordly {
// Browser playback files are transient preview inputs, never project/ROM edits.
class ScenePlaybackInputs {
  public:
    static ScenePlaybackInputs parse(const QByteArray &, const QString &assetId);
    const QJsonObject &json() const { return json_; }
    QJsonValue known(const QString &) const;
    QString description() const;
    bool captured() const { return known("capturedSnapshot").isObject(); }
    int mode(int fallback) const;
    void configure(ScenePlaybackStart &, SceneCameras &) const;
    void supply(ScenePlayback &) const;
    std::optional<quint16> controller(int update) const;
    QString boundary() const;
  private:
    QJsonObject json_;
};
} // namespace lordly
