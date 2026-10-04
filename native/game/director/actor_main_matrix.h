#ifndef LORDLY_ACTOR_MAIN_MATRIX_H
#define LORDLY_ACTOR_MAIN_MATRIX_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float radians, inverse_radians;
    double pi, quarter_turn;
    double projection[2][4]; /* origin producer and size-probe producer */
    float origin[3], probe_x[3], probe_y[3];
} LordlyActorMatrixConstants;
typedef struct {
    uint8_t matrix[64];
    float position[2], scale[2];
} LordlyActorMainMatrix;
/* Mode-zero ordinary main pass only. Shadows have a separate construction path. */
int lordly_actor_main_matrix(LordlyActorMainMatrix *out, const uint8_t actor[336],
                             const uint8_t channel[88], const float camera[14], float world_scale,
                             const LordlyActorMatrixConstants *constants);
#ifdef __cplusplus
}
#endif
#endif
