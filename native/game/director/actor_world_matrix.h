#ifndef LORDLY_ACTOR_WORLD_MATRIX_H
#define LORDLY_ACTOR_WORLD_MATRIX_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float radians;
    double half_turn, pi, slope, intercept;
    double camera_pi[2], camera_offset[2];
    double point_half_turn, point_pi, round_threshold;
} LordlyActorWorldConstants;
typedef struct {
    float point[3];
    /* Two native light records: X, Z, height percentage. */
    float planar[2][3];
} LordlyActorLighting;
typedef struct {
    uint8_t matrix[64], shadow[2][64];
} LordlyActorWorldMatrices;
/* Reconstructs ROM2AA1EC's camera basis and ROM2A9AD0's entire per-Actor
 * matrix producer. Terrain height is supplied by its separate scene owner.
 * Output commits only after all three matrices can be packed. */
int lordly_world_billboard(float basis[16], const float camera[14],
                           const LordlyActorWorldConstants *constants);
int lordly_actor_world_matrices(LordlyActorWorldMatrices *out, const uint8_t actor[336],
                                const float camera[14], float world_scale, int16_t depth,
                                float terrain_height, const LordlyActorLighting *lighting,
                                const LordlyActorWorldConstants *constants);
#ifdef __cplusplus
}
#endif
#endif
