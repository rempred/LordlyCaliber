#ifndef LORDLY_ACTOR_MOTION_H
#define LORDLY_ACTOR_MOTION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { float x, y, z; } LordlyActorPosition;
/* Motion is owned by a scene slot, independently of its current occupant. */
typedef struct {
    float vx, vy, vz;
    uint16_t remaining;
    uint8_t pause, retained;
    int present;
} LordlyActorMotion;
typedef struct { double tolerance_x, tolerance_z, speed_scale; } LordlyMotionConstants;
typedef int (*LordlyTerrainHeight)(void *context, int16_t x, int16_t z, float *height);
enum { LORDLY_MOTION_OK, LORDLY_MOTION_ARITHMETIC, LORDLY_MOTION_TERRAIN };
/* Adaptation of func_002A0B14. Failure leaves position and motion unchanged.
 * A null position retains the game's known-empty-slot no-operation. */
int lordly_actor_move(LordlyActorPosition *position, LordlyActorMotion *motion,
    const LordlyMotionConstants *constants, float start_x, float start_z,
    float target_x, float target_z, int32_t duration, int32_t speed,
    int mode, uint8_t flags, LordlyTerrainHeight terrain, void *context);
/* Only the planar integration/cleanup part of func_002A0EF0. Caller owns
 * scheduler eligibility, terrain and proximity services. Null actor suspends
 * the slot's retained motion rather than inventing an occupant. */
int lordly_actor_advance_planar(LordlyActorPosition *position, LordlyActorMotion *motion);
#ifdef __cplusplus
}
#endif
#endif
