#ifndef LORDLY_SCENE_PROJECTION_H
#define LORDLY_SCENE_PROJECTION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint8_t bytes[76]; } LordlySceneProjection;
/* Native mode-two allocation initialization, ROM 2A90EC. Padding is retained. */
void lordly_projection_initialize(LordlySceneProjection *);
/* Accepted func_002A91E4 adapted to explicit ownership and atomic finite bounds.
 * Inputs are translation X/Y, scale X/Y, full signed-word duration. */
int lordly_projection_start(LordlySceneProjection *, float, float, float, float, int32_t);
/* ROM2AB2A8: return to identity, using signed-halfword or computed duration. */
int lordly_projection_reset(LordlySceneProjection *, int32_t);
/* ROM 2A9258: null and zero-count objects are inactive; count is a halfword. */
int lordly_projection_advance(LordlySceneProjection *);
#ifdef __cplusplus
}
#endif
#endif
