#ifndef LORDLY_SCENE_DRAW_QUEUE_H
#define LORDLY_SCENE_DRAW_QUEUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int32_t x, y, z, depth;
    int16_t kind, index;
} LordlySceneDrawEntry;
/* Virtual target addresses used only for the original comparator's tie case.
 * These addresses are never dereferenced by the host implementation. */
typedef struct {
    uint32_t queue, temporary;
} LordlySceneDrawLayout;
typedef struct {
    int16_t kind, index; /* kind: 0 scene object, 1 Actor main, 2 Actor shadow */
} LordlySceneDrawPass;
typedef struct {
    /* Native queue kind:0/2 world object,1 Actor,3 projected object.
     * shadow=-1 means main pass,0/1 selects an Actor shadow matrix. */
    int16_t kind, index, shadow;
    uint8_t alpha;
} LordlySceneWorldDrawPass;
int lordly_scene_mode_two_passes(LordlySceneWorldDrawPass out[852], int *count,
                                 const LordlySceneDrawEntry *queue, int queue_count,
                                 const uint8_t *const actors[28], const uint8_t *const objects[256],
                                 const int16_t shadow_alpha[2]);
int lordly_scene_draw_queue(LordlySceneDrawEntry out[284], int *count,
                            const uint8_t *const actors[28], const uint8_t *const objects[256],
                            int object_count, const float camera[14], LordlySceneDrawLayout);
int lordly_scene_mode_zero_passes(LordlySceneDrawPass out[568], int *count,
                                  const LordlySceneDrawEntry *queue, int queue_count,
                                  const uint8_t *const actors[28], uint8_t object_channel,
                                  uint8_t channel);
#ifdef __cplusplus
}
#endif
#endif
