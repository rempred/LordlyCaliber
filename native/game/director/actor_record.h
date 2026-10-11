#ifndef LORDLY_ACTOR_RECORD_H
#define LORDLY_ACTOR_RECORD_H
#include "actor_pose.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { LORDLY_ACTOR_RECORD_BYTES = 0x150 };
typedef struct { uint8_t bytes[LORDLY_ACTOR_RECORD_BYTES]; } LordlyActorRecord;
typedef struct {
    int32_t slot, bank, animation, facing, variant;
    LordlyActorPosition position;
    uint8_t depth, presentation;
} LordlyActorSelection;
typedef int (*LordlyActorResolve)(void *, int32_t bank, int32_t animation, int32_t facing, int *state);
typedef int (*LordlyActorHeight)(void *, int mode, float x, float z, float *height);
typedef struct {
    int32_t slot, source, owner, state, flag_b, flag_a;
    uint8_t keep_ground, keep_identity, presentation;
} LordlyActorBodySelection;
/* Body initializer's record boundary (ROM 2A9364). Linked classification is
 * 0 ordinary, 1 primary, 2 secondary. Caller owns row allocation, appearance
 * resource preparation, immediate pose and propagation to linked secondaries. */
int lordly_actor_body_select(LordlyActorRecord *, const LordlyActorBodySelection *,
    int classification, int *advance);
enum { LORDLY_ACTOR_OK, LORDLY_ACTOR_INVALID, LORDLY_ACTOR_RESOURCE, LORDLY_ACTOR_TERRAIN };
void lordly_actor_read_pose(const LordlyActorRecord *, LordlyActorPose *);
void lordly_actor_write_pose(LordlyActorRecord *, const LordlyActorPose *);
/* Adaptation of accepted func_002A08C0 up to its immediate pose invocation.
 * advance becomes 1 only when that invocation is required. A failed host
 * dependency leaves the raw record unchanged. The caller must execute pose. */
int lordly_actor_select(LordlyActorRecord *, const LordlyActorSelection *,
    LordlyActorResolve, void *, int *advance);
/* Direct cleared-record creation (accepted func_0029D790), with the ordinary
 * initializer reconstructed from ROM 0x29DC0C..0x29DEC8. Caller performs the
 * immediate pose invocation after construction. */
int lordly_actor_construct(LordlyActorRecord *, const LordlyActorSelection *, int mode,
    LordlyActorResolve, LordlyActorHeight, void *);
#ifdef __cplusplus
}
#endif
#endif
