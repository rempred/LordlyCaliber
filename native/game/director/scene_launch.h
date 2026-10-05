#ifndef LORDLY_SCENE_LAUNCH_H
#define LORDLY_SCENE_LAUNCH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int16_t resource_class, environment, image_row, capture;
} LordlySceneLaunch;
/* Raw-word pre-scan from4ED60. The reference C at Decomp main62cc7ef4 is
 * HYBRID_C; this host adaptation has no assembler. Unwritten inputs survive. */
int lordly_scene_launch_scan(const uint32_t *words, int count, LordlySceneLaunch *launch);
#ifdef __cplusplus
}
#endif
#endif
