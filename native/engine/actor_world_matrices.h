#pragma once
#include "engine/scene_camera.h"
#include "game/director/actor_world_matrix.h"
namespace lordly {
struct ActorWorldMatrices {
    QByteArray matrix;
    std::array<QByteArray, 2> shadows;
    static ActorWorldMatrices prepare(const Rom &, const QByteArray &actor, const SceneCamera &,
                                      qint16 depth, float terrainHeight,
                                      const LordlyActorLighting &);
};
} // namespace lordly
