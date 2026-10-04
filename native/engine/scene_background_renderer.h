#pragma once
#include "engine/actor_renderer.h"
#include "engine/scene_images.h"
namespace lordly {
struct SceneBackgroundRecord {
    QPoint origin;
    QImage image;
};
struct SceneBackgroundChannel {
    QString imageId;
    QVector<SceneBackgroundRecord> records;
};
class SceneBackgroundRenderer {
  public:
    static SceneBackgroundRenderer group(const Rom &, int selector);
    int channelCount() const {
        return channels_.size();
    }
    const SceneBackgroundChannel &channel(int index) const;
    QVector<ActorRasterLayer> project(int channel, const SceneCamera &,
                                      const QByteArray &parent) const;
    QString description() const;

  private:
    QVector<SceneBackgroundChannel> channels_;
};
} // namespace lordly
