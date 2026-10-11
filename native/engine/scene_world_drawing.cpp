#include "engine/scene_world_drawing.h"
#include "game/director/scene_matrix.h"
namespace lordly {
namespace {
QByteArray pack(const float matrix[16]) {
    QByteArray result(64, 0);
    if (!lordly_matrix_pack(reinterpret_cast<uint8_t *>(result.data()), matrix))
        throw Error("World drawing matrix exceeds the native fixed-point range");
    return result;
}
} // namespace
QByteArray SceneWorldDrawing::clipMatrix() {
    float m[16];
    lordly_matrix_identity(m);
    // guOrtho's float operation order, with ROM1FC3BC's arguments.
    m[0] = 2.f / 320.f;
    m[5] = 2.f / 240.f;
    m[10] = -2.f / 1999.f;
    m[14] = -2001.f / 1999.f;
    return pack(m);
}
QByteArray SceneWorldDrawing::modelMatrix(const SceneProjection &projection) {
    float scale[16], translation[16], result[16];
    lordly_matrix_scale(scale, projection.scaleX(), projection.scaleY(), 1);
    lordly_matrix_translate(translation, projection.translateX(), projection.translateY(), -100);
    const auto s = pack(scale), t = pack(translation);
    lordly_matrix_unpack(scale, reinterpret_cast<const uint8_t *>(s.constData()));
    lordly_matrix_unpack(translation, reinterpret_cast<const uint8_t *>(t.constData()));
    lordly_matrix_multiply(result, scale, translation);
    return pack(result);
}
} // namespace lordly
