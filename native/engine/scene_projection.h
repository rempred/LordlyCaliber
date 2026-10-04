#pragma once
#include "engine/rom.h"
#include "game/director/scene_projection.h"
#include <QVector>
namespace lordly {
class SceneProjection {
  public:
    SceneProjection();
    explicit SceneProjection(const QByteArray &record);
    QByteArray record() const;
    float scaleX() const;
    float scaleY() const;
    float translateX() const;
    float translateY() const;
    int remaining() const;
    void start(float x, float y, float scaleX, float scaleY, qint32 duration);
    void advance();
    bool service(int mode, quint32 opcode, const QVector<quint32> &operands);

  private:
    LordlySceneProjection raw_{};
};
} // namespace lordly
