/* Portable reconstruction of mode-zero prepass ROM 2AE654 and the main-pass
 * portion of 2AEB48 ending at 2AED04. Original reference: Decomp 62cc7ef4.
 * Also reconstructs CPU projection 29CBDC/2A1484 and look-at 20D40..210C0.
 * Host trigonometry replaces libm. This is not accepted matching C. */
#include "actor_main_matrix.h"
#include "scene_matrix.h"
#include <math.h>
#include <string.h>
static float real(const uint8_t *p) {
    const uint32_t bits = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static float dot(const float a[3], const float b[3]) {
    return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2];
}
static void cross(float out[3], const float a[3], const float b[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}
static int normalize(float v[3], double numerator) {
    const float length = sqrtf(dot(v, v));
    const float scale = (float)(numerator / (double)length);
    int i;
    if (!isfinite(scale))
        return 0;
    for (i = 0; i < 3; ++i)
        v[i] *= scale;
    return 1;
}
static int look_at(float out[16], const float camera[14]) {
    float back[3], right[3], up[3];
    int i;
    for (i = 0; i < 3; ++i)
        back[i] = camera[8 + i] - camera[5 + i];
    if (!normalize(back, -1.))
        return 0;
    cross(right, camera + 11, back);
    if (!normalize(right, 1.))
        return 0;
    cross(up, back, right);
    if (!normalize(up, 1.))
        return 0;
    lordly_matrix_identity(out);
    for (i = 0; i < 3; ++i) {
        out[i * 4] = right[i];
        out[i * 4 + 1] = up[i];
        out[i * 4 + 2] = back[i];
    }
    out[12] = -dot(camera + 5, right);
    out[13] = -dot(camera + 5, up);
    out[14] = -dot(camera + 5, back);
    return 1;
}
static int quantize(float matrix[16]) {
    uint8_t packed[64];
    if (!lordly_matrix_pack(packed, matrix))
        return 0;
    lordly_matrix_unpack(matrix, packed);
    return 1;
}
static int concatenate(float out[16], const float left[16], const float right[16]) {
    lordly_matrix_multiply(out, left, right);
    return quantize(out);
}
static int project(float out[2], const float sample[3], const float model[16], const float view[16],
                   float fov, const double projection[4]) {
    float combined[16], point[3];
    const float angle = (float)((double)fov * projection[0] / projection[1]);
    float divisor;
    int i;
    lordly_matrix_multiply(combined, model, view);
    for (i = 0; i < 3; ++i)
        point[i] = ((sample[0] * combined[i] + sample[1] * combined[4 + i]) +
                    sample[2] * combined[8 + i]) +
                   combined[12 + i];
    divisor = point[2] * sinf(angle);
    divisor /= cosf(angle);
    if (divisor == 0.f) {
        out[0] = point[0];
        out[1] = point[1];
    } else {
        const float horizontal = point[0] * 160.f;
        out[0] = (float)(projection[3] - (double)horizontal / ((double)divisor * projection[2]));
        out[1] = 120.f - (point[1] * 120.f) / divisor;
    }
    return isfinite(out[0]) && isfinite(out[1]);
}
int lordly_actor_main_matrix(LordlyActorMainMatrix *out, const uint8_t actor[336],
                             const uint8_t channel[88], const float camera[14], float world_scale,
                             const LordlyActorMatrixConstants *c) {
    LordlyActorMainMatrix result;
    float view[16], basis[16], model[16], temporary[16], reference[2];
    float angle, tilt, yaw, dx, dy, dz;
    int i;
    for (i = 0; i < 14; ++i)
        if (!isfinite(camera[i]))
            return 0;
    if (!isfinite(world_scale) || !look_at(view, camera))
        return 0;
    dx = camera[8] - camera[5];
    dy = camera[9] - camera[6];
    dz = camera[10] - camera[7];
    angle = atan2f(sqrtf(dz * dz + dx * dx), dy) * 180.f;
    tilt = (float)(c->quarter_turn - (double)angle / c->pi);
    angle = atan2f(dx, dz) * 180.f;
    yaw = (float)((double)angle / c->pi) - 180.f;
    lordly_matrix_rotation(basis, tilt, yaw, 0.f, c->radians);
    if (!quantize(basis))
        return 0;
    lordly_matrix_scale(model, real(actor + 0x104), real(actor + 0x108), real(actor + 0x10c));
    if (!quantize(model))
        return 0;
    lordly_matrix_translate(temporary, real(actor + 0x11c), 0.f, real(actor + 0x124));
    if (!quantize(temporary) || !concatenate(model, model, temporary))
        return 0;
    lordly_matrix_scale(temporary, world_scale, world_scale, world_scale);
    if (!quantize(temporary) || !concatenate(model, model, temporary) ||
        !concatenate(model, basis, model))
        return 0;
    if (!project(result.position, c->origin, model, view, camera[0], c->projection[0]) ||
        !project(reference, c->probe_x, model, view, camera[0], c->projection[1]))
        return 0;
    result.scale[0] = fabsf(reference[0] - result.position[0]) / c->probe_x[0];
    if (!project(reference, c->probe_y, model, view, camera[0], c->projection[1]))
        return 0;
    result.scale[1] = fabsf(reference[1] - result.position[1]) / c->probe_y[1];
    lordly_matrix_scale(model, real(actor + 0x104) * result.scale[0],
                        real(actor + 0x108) * result.scale[1], real(actor + 0x10c));
    if (!quantize(model))
        return 0;
    lordly_matrix_translate(temporary, 0.f, real(actor + 0x120), 0.f);
    if (!quantize(temporary) || !concatenate(model, model, temporary))
        return 0;
    // ROM 10FE0's two-angle form is the inverse of the channel RPY matrix.
    lordly_matrix_rotation(temporary, real(channel + 0x48), real(channel + 0x4c), 0.f,
                           c->inverse_radians);
    for (i = 0; i < 4; ++i) {
        int j;
        for (j = i + 1; j < 4; ++j) {
            const float value = temporary[i * 4 + j];
            temporary[i * 4 + j] = temporary[j * 4 + i];
            temporary[j * 4 + i] = value;
        }
    }
    if (!quantize(temporary) || !concatenate(model, model, temporary))
        return 0;
    lordly_matrix_translate(temporary, result.position[0] - 160.f, result.position[1] - 120.f, 0.f);
    if (!quantize(temporary) || !concatenate(model, model, temporary))
        return 0;
    lordly_matrix_unpack(temporary, channel);
    lordly_matrix_multiply(model, model, temporary);
    if (!lordly_matrix_pack(result.matrix, model))
        return 0;
    *out = result;
    return 1;
}
