#ifndef LORDLY_SCENE_TRANSFORM_H
#define LORDLY_SCENE_TRANSFORM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Channel record order is native: matrix[64], translations X/Y, rotations X/Y,
 * translation Z and uniform scale. Deltas follow the six source-array order. */
void lordly_transform_identity(uint8_t records[20][88]);
int lordly_transform_prepare(uint8_t records[20][88], float delta[20][6],
                             const uint8_t current[480], const uint8_t *target, int32_t duration);
int lordly_transform_advance(uint8_t records[20][88], const float delta[20][6]);
int lordly_transform_done(int job_present);
#ifdef __cplusplus
}
#endif
#endif
