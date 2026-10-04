#pragma once
#include "engine/scene_camera.h"
namespace lordly {
struct ActorMainMatrix {
    QByteArray matrix;
    QPointF position, scale;
    static ActorMainMatrix prepare(const Rom &, const QByteArray &actor, const QByteArray &channel,
                                   const SceneCamera &registered);
};
} // namespace lordly
