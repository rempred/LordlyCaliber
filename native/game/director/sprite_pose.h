#ifndef LORDLY_SPRITE_POSE_H
#define LORDLY_SPRITE_POSE_H
#include "actor_pose.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The 2D sprite sequencer uses signed integer positions, not Actor floats. */
typedef struct {
    int32_t x, y, bank, cursor, delay, frame;
    int16_t state;
    uint8_t variant, color[16], material[16];
} LordlySpritePose;
typedef int (*LordlySpriteLookup)(void *, const LordlySpritePose *, LordlyPoseRecord *);
typedef int (*LordlySpriteSound)(void *, const LordlySpritePose *, const LordlyPoseRecord *);
/* Portable reconstruction of ROM 29E7E0..29EAB0, live RAM 8022EFB0.
 * Uses the shared pose statuses and resumable invocation, but no Actor effects. */
int lordly_sprite_advance_pose(LordlySpritePose *, LordlyPoseInvocation *, int budget,
    LordlySpriteLookup, LordlySpriteSound, void *context);
#ifdef __cplusplus
}
#endif
#endif
