/* Portable reconstruction from original assembly at Decomp62cc7ef4:
 * ROM2A9AD0,2AA1EC,29D158,29D1B8 and resident11170/2CB50.
 * Host trigonometry replaces target libm. Not accepted matching C. */
#include "actor_world_matrix.h"
#include "scene_matrix.h"
#include <math.h>
#include <string.h>
static float real(const uint8_t *p) {
    uint32_t bits = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static int rounded(float value, double threshold, int32_t *out) {
    int32_t whole;
    if (!isfinite(value) || value < -2147483648.f || value >= 2147483648.f)
        return 0;
    whole = (int32_t)value;
    /* Original helper compares a signed fractional part strictly above0.5.
     * Negative values truncate; this is deliberately not roundf. */
    if ((double)(float)((double)value - (double)whole) > threshold)
        ++whole;
    *out = whole;
    return 1;
}
static float planar_angle(float x, float z, const float light[3],
                          const LordlyActorWorldConstants *c) {
    float angle;
    if (z != light[1]) {
        const float ratio = (x - light[0]) / (z - light[1]);
        double degrees = (double)atanf(ratio) * c->half_turn / c->pi;
        if (z > light[1])
            degrees += c->half_turn;
        angle = (float)degrees;
    } else
        angle = x < light[0] ? 90.f : -90.f;
    if (angle < 0.f)
        angle += 360.f;
    return angle;
}
int lordly_actor_world_matrices(LordlyActorWorldMatrices *out, const uint8_t actor[336],
                                const float camera[14], float world_scale, int16_t depth,
                                float terrain_height, const LordlyActorLighting *lighting,
                                const LordlyActorWorldConstants *c) {
    LordlyActorWorldMatrices result;
    float basis[16], model[16], ground[16], temporary[16], scale[16];
    float x = real(actor + 0x11c), y = real(actor + 0x120), z = real(actor + 0x124);
    float ground_y, angle, ratio, dx, dy, dz, tilt, yaw;
    int i, count;
    for (i = 0; i < 14; ++i)
        if (!isfinite(camera[i]))
            return 0;
    for (i = 0x104; i <= 0x130; i += 4)
        if (!isfinite(real(actor + i)))
            return 0;
    for (i = 0; i < 3; ++i)
        if (!isfinite(lighting->point[i]) || !isfinite(lighting->planar[0][i]) ||
            !isfinite(lighting->planar[1][i]))
            return 0;
    if (!isfinite(world_scale) || !isfinite(terrain_height))
        return 0;
    /* Camera owner branches on eye Z, preserving its float operation order. */
    if (camera[7] < 0.f) {
        dx = camera[8] - camera[5];
        dy = camera[9] - camera[6];
        dz = camera[10] - camera[7];
        angle = atan2f(sqrtf(dy * dy + dx * dx), dz) * 180.f;
        tilt = (float)((double)angle / c->camera_pi[0] + c->camera_offset[0]);
        angle = atan2f(dx, dz) * 180.f;
        yaw = (float)((double)angle / c->camera_pi[0]) - 180.f;
    } else {
        dx = camera[5] - camera[8];
        dy = camera[6] - camera[9];
        dz = camera[7] - camera[10];
        angle = atan2f(sqrtf(dy * dy + dx * dx), dz) * 180.f;
        tilt = (float)((double)angle / c->camera_pi[1] + c->camera_offset[1]);
        angle = atan2f(dx, dz) * 180.f;
        yaw = (float)((double)angle / c->camera_pi[1]);
    }
    lordly_matrix_rotation(basis, tilt, yaw, 0.f, c->radians);
    ground_y = actor[0x145] & 1   ? terrain_height
               : actor[0x145] & 4 ? real(actor + 0x128) + y
               : actor[0x145] & 2 ? real(actor + 0x128)
                                  : y;
    lordly_matrix_scale(model, real(actor + 0x104), real(actor + 0x108), real(actor + 0x10c));
    lordly_matrix_translate(temporary, x, ground_y, z + (float)depth);
    lordly_matrix_multiply(ground, model, temporary);
    lordly_matrix_translate(temporary, x, y, z + (float)depth);
    lordly_matrix_multiply(model, model, temporary);
    lordly_matrix_rotation(temporary, real(actor + 0x110), real(actor + 0x114), real(actor + 0x118),
                           c->radians);
    lordly_matrix_multiply(ground, ground, temporary);
    lordly_matrix_multiply(model, model, temporary);
    lordly_matrix_scale(scale, world_scale, world_scale, world_scale);
    lordly_matrix_multiply(ground, ground, scale);
    lordly_matrix_multiply(model, model, scale);
    lordly_matrix_multiply(model, basis, model);
    if (!lordly_matrix_pack(result.matrix, model))
        return 0;
    count = lighting->point[1] == 0.f && real(actor + 0x12c) == 0.f && real(actor + 0x130) == 1.f
                ? 2
                : 1;
    for (i = 0; i < count; ++i) {
        if (count == 2) {
            const float *light = lighting->planar[i];
            angle = planar_angle(x, z, light, c);
            ratio = light[2] < 0.f ? 1.f
                    : light[2] > 100.f
                        ? 0.5f
                        : (float)((double)(100.f - light[2]) * c->slope + c->intercept);
        } else if (real(actor + 0x12c) != 0.f || real(actor + 0x130) != 1.f) {
            angle = real(actor + 0x12c);
            ratio = real(actor + 0x130);
        } else {
            int32_t actor_height, light_height;
            dx = x - lighting->point[0];
            dz = lighting->point[2] - z;
            angle = (float)((double)atan2f(dx, dz) * c->point_half_turn / c->point_pi) + 180.f;
            if (angle < 0.f)
                angle += 360.f;
            if (angle >= 360.f)
                angle -= 360.f;
            ratio = 0.f;
            if (lighting->point[1] > 0.f) {
                if (!rounded(y, c->round_threshold, &actor_height) ||
                    !rounded(lighting->point[1], c->round_threshold, &light_height))
                    return 0;
                ratio = 6.f;
                if (actor_height != light_height) {
                    dz = z - lighting->point[2];
                    ratio = sqrtf(dx * dx + dz * dz) / (lighting->point[1] - y);
                }
                ratio = fabsf(ratio);
                if (ratio > 5.f)
                    ratio = 5.f;
            }
        }
        lordly_matrix_scale(model, angle >= 150.f && angle < 330.f ? -1.f : 1.f, ratio, 1.f);
        lordly_matrix_rotation(temporary, -90.f, angle, 0.f, c->radians);
        lordly_matrix_multiply(model, model, temporary);
        lordly_matrix_multiply(model, model, ground);
        if (!lordly_matrix_pack(result.shadow[i], model))
            return 0;
    }
    if (count == 1)
        memcpy(result.shadow[1], result.shadow[0], 64);
    *out = result;
    return 1;
}
