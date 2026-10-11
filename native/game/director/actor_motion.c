#include "actor_motion.h"
#include <math.h>
#include <stddef.h>

static int signed_half(uint32_t value) {
    value &= 65535U;
    return value < 32768U ? (int)value : (int)value - 65536;
}

int lordly_actor_move(LordlyActorPosition *position, LordlyActorMotion *motion,
    const LordlyMotionConstants *constants, float start_x, float start_z,
    float target_x, float target_z, int32_t duration, int32_t speed,
    int mode, uint8_t flags, LordlyTerrainHeight terrain, void *context) {
    LordlyActorPosition p;
    LordlyActorMotion m;
    float x, z, dx, dz, denominator, squared;
    double tolerance;
    int reached = 0;
    if (!position) return LORDLY_MOTION_OK;
    if (!motion || !constants) return LORDLY_MOTION_ARITHMETIC;
    p = *position;
    m = *motion;
    if (!isfinite(start_x) || !isfinite(start_z) || !isfinite(target_x) ||
        !isfinite(target_z) || !isfinite(p.x) || !isfinite(p.z) ||
        !isfinite(constants->tolerance_x) || !isfinite(constants->tolerance_z) ||
        !isfinite(constants->speed_scale)) return LORDLY_MOTION_ARITHMETIC;
    if (start_x == -1.0f && start_z == -1.0f) { x = p.x; z = p.z; }
    else { x = start_x; z = start_z; p.x = x; p.z = z; }
    if (duration == 1) {
        p.x = target_x; p.z = target_z;
        if (mode == 2 && (flags & 1)) {
            if (!terrain) return LORDLY_MOTION_TERRAIN;
            if ((double)p.x < -2147483648.0 || (double)p.x >= 2147483648.0 ||
                (double)p.z < -2147483648.0 || (double)p.z >= 2147483648.0)
                return LORDLY_MOTION_ARITHMETIC;
            if (!terrain(context, (int16_t)signed_half((uint32_t)(int32_t)p.x),
                (int16_t)signed_half((uint32_t)(int32_t)p.z), &p.y))
                return LORDLY_MOTION_TERRAIN;
            if (!isfinite(p.y)) return LORDLY_MOTION_ARITHMETIC;
        }
        *position = p;
        return LORDLY_MOTION_OK;
    }
    tolerance = constants->tolerance_x;
    if ((double)x < (double)target_x + tolerance &&
        (double)target_x - tolerance < (double)x) {
        p.x = target_x; reached = 1; tolerance = constants->tolerance_z;
    }
    if ((double)z < (double)target_z + tolerance &&
        (double)target_z - tolerance < (double)z) { p.z = target_z; reached += 2; }
    if (reached == 3) { *position = p; return LORDLY_MOTION_OK; }
    dx = target_x - x; dz = target_z - z;
    if (speed) {
        float xx = dx * dx, zz = dz * dz;
        squared = xx + zz;
        if (squared == 0.0f) {
            m.vx = m.vz = 0.0f; m.remaining = 1;
            /* The original leaves the existing/allocator pause byte untouched. */
            m.present = 1; *position = p; *motion = m;
            return LORDLY_MOTION_OK;
        }
        squared = (float)((constants->speed_scale / (double)speed) * (double)sqrtf(squared));
        if (!isfinite(squared) || (double)squared < -2147483648.0 ||
            (double)squared >= 2147483648.0) return LORDLY_MOTION_ARITHMETIC;
        m.remaining = (uint16_t)(uint32_t)(int32_t)squared;
        denominator = (float)signed_half(m.remaining);
    } else {
        m.remaining = (uint16_t)(uint32_t)duration;
        denominator = (float)duration;
    }
    if (denominator == 0.0f) return LORDLY_MOTION_ARITHMETIC;
    m.vx = dx / denominator; m.vz = dz / denominator;
    if (!isfinite(m.vx) || !isfinite(m.vz)) return LORDLY_MOTION_ARITHMETIC;
    m.pause = 0; m.present = 1;
    *position = p; *motion = m;
    return LORDLY_MOTION_OK;
}

int lordly_actor_advance_planar(LordlyActorPosition *position, LordlyActorMotion *motion) {
    if (!motion || !motion->present || !position) return LORDLY_MOTION_OK;
    if (!motion->pause) {
        const float x = position->x + motion->vx, z = position->z + motion->vz;
        if (!isfinite(x) || !isfinite(z)) return LORDLY_MOTION_ARITHMETIC;
        position->x = x; position->z = z;
        motion->remaining = (uint16_t)(motion->remaining - 1U);
    }
    if (!motion->remaining) motion->present = 0;
    return LORDLY_MOTION_OK;
}
