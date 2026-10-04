#ifndef LORDLY_SCENE_SCHEDULE_H
#define LORDLY_SCENE_SCHEDULE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* These are service identities in the Actor-scene overlay, not callable host
 * addresses. Names remain neutral where the two overlay variants share a slot. */
typedef enum {
    LORDLY_SCENE_COLOR,
    LORDLY_SCENE_MODE_ZERO_1,
    LORDLY_SCENE_MODE_ZERO_2,
    LORDLY_SCENE_MODE_ZERO_3,
    LORDLY_SCENE_MODE_ZERO_4,
    LORDLY_SCENE_MODE_ZERO_5,
    LORDLY_SCENE_MODE_ZERO_6,
    LORDLY_SCENE_MODE_TWO_PREPARE,
    LORDLY_SCENE_COUNTER,
    LORDLY_SCENE_HEIGHT,
    LORDLY_SCENE_MOVEMENT,
    LORDLY_SCENE_FACING,
    LORDLY_SCENE_POSES,
    LORDLY_SCENE_SPECIAL_BLACK,
    LORDLY_SCENE_RESOURCE_CACHE,
    LORDLY_SCENE_PROJECTION,
    LORDLY_SCENE_MODE_TWO_4,
    LORDLY_SCENE_DIRECTOR,
    LORDLY_SCENE_AFTER_DIRECTOR,
    LORDLY_SCENE_PATHS,
    LORDLY_SCENE_TRANSITION,
    LORDLY_SCENE_DELAYED_CALLBACK,
    LORDLY_SCENE_SERVICE_COUNT
} LordlySceneService;

typedef struct {
    uint8_t mode, alternate, delayed_counter;
    /* Host continuation only: native execution performs one whole invocation. */
    int active, phase;
    int executed;
} LordlySceneSchedule;

enum { LORDLY_SCENE_COMPLETE, LORDLY_SCENE_UNAVAILABLE, LORDLY_SCENE_BUDGET, LORDLY_SCENE_INVALID };
/* Returning zero retains this stage. A resumed service owns any partial work;
 * previous successful stages are never called again in the same invocation. */
typedef int (*LordlySceneInvoke)(void *, LordlySceneService);
int lordly_scene_schedule(LordlySceneSchedule *, LordlySceneInvoke, void *, int budget);
uint32_t lordly_scene_service_address(LordlySceneService);
#ifdef __cplusplus
}
#endif
#endif
