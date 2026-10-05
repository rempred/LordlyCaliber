#ifndef LORDLY_IMAGE_MOTION_H
#define LORDLY_IMAGE_MOTION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float step_x, step_y;
    int32_t duration;
    uint32_t elapsed;
} LordlyImageMotion;
typedef struct {
    uint8_t state;
    uint32_t elapsed, duration;
} LordlyImageZoom;
/* Start adapts accepted func_002B4D58.c, Decomp main62cc7ef4. Other routines
 * reconstruct original2B3FE4/2B4FB4. No target pointers or allocators on PC.
 * Nonfinite arithmetic rejects without modifying the supplied records. */
int lordly_image_motion_start(LordlyImageMotion *, LordlyImageZoom *,
                              const int32_t operands[7], float current_x, float current_y);
int lordly_image_zoom_advance(LordlyImageZoom *, float *scale, const double constants[4]);
/* Returns0 if released,1 if retained,-1 if arithmetic is invalid. */
int lordly_image_motion_advance(LordlyImageMotion *, LordlyImageZoom *,
                                float *x, float *y, float *scale, const double constants[4]);
#ifdef __cplusplus
}
#endif
#endif
