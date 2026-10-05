#pragma once
#include "engine/actor_renderer.h"
#include "engine/scene_images.h"
namespace lordly {
struct SceneBackgroundRecord {
    QPoint origin;
    QImage image;
    QSize extent;
};
struct SceneBackgroundChannel {
    QString imageId;
    QVector<SceneBackgroundRecord> records;
};
class SceneBackgroundRenderer {
  public:
    static SceneBackgroundRenderer group(const Rom &, int selector);
    // Independent selectors; channels zero/one are the base/foreground passes.
    static SceneBackgroundRenderer world(const Rom &, int environment, int foreground);
    int channelCount() const {
        return channels_.size();
    }
    const SceneBackgroundChannel &channel(int index) const;
    QVector<ActorRasterLayer> project(int channel, const SceneCamera &, const QByteArray &parent,
                                      RenderColor primitive = {255, 255, 255, 255}) const;
    QString description() const;

  private:
    QVector<SceneBackgroundChannel> channels_;
};
} // namespace lordly
