#pragma once
#include "engine/scene_camera.h"
namespace lordly {
struct ActorMatrices {
    QByteArray matrix, shadow;
    QPointF position, scale;
    static ActorMatrices prepare(const Rom &, const QByteArray &actor, const QByteArray &channel,
                                 const SceneCamera &registered);
};
} // namespace lordly
