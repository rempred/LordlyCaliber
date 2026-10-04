#pragma once
#include "engine/scene_camera.h"
#include "game/director/scene_draw_queue.h"
namespace lordly {
class SceneDrawQueue {
  public:
    static SceneDrawQueue build(const std::array<QByteArray, 28> &actors,
                                const QVector<QByteArray> &objects, const SceneCamera &camera,
                                LordlySceneDrawLayout layout);
    const QVector<LordlySceneDrawEntry> &entries() const {
        return entries_;
    }
    QByteArray record() const;
    static SceneDrawQueue fromRecord(const QByteArray &);
    QVector<LordlySceneDrawPass> modeZeroPasses(const std::array<QByteArray, 28> &actors,
                                                quint8 objectChannel, quint8 channel) const;

  private:
    QVector<LordlySceneDrawEntry> entries_;
};
} // namespace lordly
