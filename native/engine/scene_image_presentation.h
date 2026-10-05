#pragma once
#include "engine/actor_renderer.h"
#include "engine/scene_images.h"
namespace lordly {
// Immutable source pixels and snapshot-owned processed pixels. The playback
// scheduler owns the associated 0x848-byte state shared with image motion.
class SceneImagePresentation {
  public:
    static SceneImagePresentation load(const Rom &, int row);
    QByteArray initialContext() const;
    void construct(const Rom &, const QVector<quint32> &, const SceneCamera &, QByteArray &context);
    static QImage vignette(const QImage &, int alphaCap);
    int slot() const {
        return slot_;
    }
    const QString &imageId() const {
        return imageId_;
    }
    const QImage &source() const {
        return source_;
    }
    const QImage &processed() const {
        return processed_;
    }
    QVector<ActorRasterLayer> project(const QByteArray &context, const SceneCamera &,
                                      const QByteArray &parent) const;

  private:
    QString imageId_;
    QImage source_, processed_;
    int slot_ = -1;
};
} // namespace lordly
