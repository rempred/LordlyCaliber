#pragma once
#include "engine/scene_camera.h"
#include "engine/scene_projection.h"
namespace lordly {
struct SceneWorldDrawing {
    // ROM1FC3BC orthographic volume and ROM2AC14C's shared 2D model matrix.
    static QByteArray clipMatrix();
    static QByteArray modelMatrix(const SceneProjection &);
};
} // namespace lordly
