#ifndef LORDLY_ACTOR_POSE_H
#define LORDLY_ACTOR_POSE_H
#include "actor_motion.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    LordlyActorPosition position;
    int32_t bank, owner, cursor, delay, frame;
    int16_t state, previous_state, flag_b;
    uint8_t decoder_mode, variant, material[16], material_delta[16];
} LordlyActorPose;
typedef struct { uint8_t opcode, operands[3]; } LordlyPoseRecord;
/* 1 means supplied; 0 means unavailable. Callbacks must leave external state
 * unchanged when declining. Exhausted counted programs supply opcode zero. */
typedef int (*LordlyPoseLookup)(void *context, const LordlyActorPose *actor, LordlyPoseRecord *record);
typedef int (*LordlyPoseShared)(void *context, const LordlyActorPose *actor, const LordlyPoseRecord *record);
typedef int (*LordlyPoseRegistered)(void *context, const LordlyActorPose *actor);
typedef int (*LordlyPoseFixed)(void *context, const LordlyActorPose *actor);
typedef struct { int active; uint8_t result; } LordlyPoseInvocation;
enum { LORDLY_POSE_OK, LORDLY_POSE_BUDGET, LORDLY_POSE_LOOKUP,
       LORDLY_POSE_SHARED, LORDLY_POSE_INVALID };
/* Portable reconstruction of ordinary effects in ROM 0x29E218..0x29E70C,
 * live RAM 0x8022E9E8. One call completes an eligible sequencer invocation,
 * or retains invocation state at a bounded host suspension. Not a video tick. */
int lordly_actor_advance_pose(LordlyActorPose *actor, LordlyPoseInvocation *invocation,
    int budget, LordlyPoseLookup lookup, LordlyPoseShared shared, void *context);
int lordly_actor_advance_pose_mapped(LordlyActorPose *, LordlyPoseInvocation *, int budget,
    LordlyPoseLookup, LordlyPoseShared, void *context, LordlyPoseRegistered, LordlyPoseFixed);
#ifdef __cplusplus
}
#endif
#endif
