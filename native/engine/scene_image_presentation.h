#pragma once
#include "engine/actor_renderer.h"
#include "engine/scene_images.h"
#include "game/director/image_echo.h"
namespace lordly {
struct SceneImageDraw {
    QByteArray matrix;
    quint8 alpha;
    bool current;
};
// Immutable source pixels and snapshot-owned processed pixels. The playback
// scheduler owns the associated 0x848-byte state shared with image motion.
class SceneImagePresentation {
  public:
    static SceneImagePresentation load(const Rom &, int row);
    QByteArray initialContext() const;
    void construct(const Rom &, const QVector<quint32> &, const SceneCamera &, QByteArray &context);
    void startEcho(const QVector<quint32> &, QByteArray &context, QByteArray &channel);
    void advanceEcho(const Rom &, QByteArray &context);
    bool contains(int channel) const {
        return images_.contains(channel);
    }
    int channelCount() const {
        return images_.isEmpty() ? 0 : images_.lastKey() + 1;
    }
    const LordlyImageEcho &echo() const {
        return echo_;
    }
    static QVector<SceneImageDraw> drawPlan(const QByteArray &context, const LordlyImageEcho &);
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
    QVector<ActorRasterLayer> project(int channel, const QByteArray &context, const SceneCamera &,
                                      const QByteArray &parent) const;

  private:
    QString imageId_;
    QImage source_, processed_;
    int slot_ = -1;
    QMap<int, bool> images_; // false=original, true=processed; mirrors native image slots.
    LordlyImageEcho echo_{};
};
} // namespace lordly
