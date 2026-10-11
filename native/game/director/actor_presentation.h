#ifndef LORDLY_ACTOR_PRESENTATION_H
#define LORDLY_ACTOR_PRESENTATION_H
#include "actor_motion.h"
#ifdef __cplusplus
extern "C" {
#endif
float lordly_body_speed(uint8_t source_owner, int32_t owner_context,
                       uint8_t source_entry, uint8_t owner_entry);
/* ROM 2A0A68: bit 0 means X snapped, bit 1 means Z snapped. */
int lordly_actor_snap(LordlyActorPosition *, float target_x, float target_z,
                     double tolerance_x, double tolerance_z);
#ifdef __cplusplus
}
#endif
#endif
