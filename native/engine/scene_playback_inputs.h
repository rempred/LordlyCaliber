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
    QJsonObject services() const;
    quint16 textSpeed(quint16 fallback) const;
    bool automaticPages() const;
    int choice(int index) const;
    std::array<qint16, 3> globalColor() const;
    bool omitBackground() const;
    QString description() const;
    bool captured() const { return known("capturedSnapshot").isObject(); }
    int resumeLimit() const { return known("capturedResume").toObject()["updates"].toInt(-1); }
    int updateLimit() const;
    int mode(int fallback) const;
    void configure(ScenePlaybackStart &, SceneCameras &) const;
    void validatePoseRegistry(const Rom &, BodyAnimationData::Pointer = {}) const;
    void supply(ScenePlayback &) const;
    std::optional<quint16> controller(int update) const;
    quint16 direction(int update) const;
    quint16 history(int update) const;
    QString boundary() const;
  private:
    QJsonObject json_;
    struct Control { int update; quint16 action, direction, history; };
    QVector<Control> controls_;
    const Control *control(int update) const;
};
} // namespace lordly
