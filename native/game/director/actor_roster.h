#ifndef LORDLY_ACTOR_ROSTER_H
#define LORDLY_ACTOR_ROSTER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Host adaptation of accepted func_002ABB3C (8012b84a). The original source
 * is retained beside this file; target pointers and services are host-owned. */
int lordly_roster_select(int32_t wanted[2], uint32_t flags, int32_t source);
int lordly_roster_same(uint8_t a, uint8_t b);
int lordly_roster_pose_offset(uint32_t flags, int32_t depth, int context, int scenario,
                              int variant);
/* Reconstructed ROM2AB574 leader normalizer predicate; caller walks the
 * mutable slot table and swaps Actor identities, leaving slot-owned jobs. */
int lordly_roster_leader_slot(uint32_t flags, int32_t owner, int pose, int private_primary);
#ifdef __cplusplus
}
#endif
#endif
