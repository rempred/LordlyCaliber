#ifndef LORDLY_DIRECT_POSE_H
#define LORDLY_DIRECT_POSE_H
#include "actor_pose.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Opaque big-endian 80-byte native descriptor pose record. */
typedef int (*LordlyDirectLookup)(void *, const uint8_t *, LordlyPoseRecord *);
typedef int (*LordlyDirectSound)(void *, const uint8_t *, const LordlyPoseRecord *);
typedef uint32_t (*LordlyDirectRandom)(void *);
int lordly_direct_initialize(uint8_t *record, uint32_t selector,
                            LordlyDirectRandom random, void *context);
int lordly_direct_reset(uint8_t *record, uint32_t selector);
int lordly_direct_advance(uint8_t *record, LordlyPoseInvocation *, int budget,
                         LordlyDirectLookup, LordlyDirectSound, LordlyDirectRandom, void *context);
#ifdef __cplusplus
}
#endif
#endif
