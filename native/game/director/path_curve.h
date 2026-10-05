#ifndef LORDLY_PATH_CURVE_H
#define LORDLY_PATH_CURVE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Reconstructed from Rev0 ROM2A1860/2A1CF4/2A1EC4. Caller owns arrays.
   Native interpolation excludes the final endpoint. Reject degenerate input. */
int lordly_path_sample(float *out_x, float *out_y, int samples, int points, const float *x,
                       const float *y, double time_scale);
/* Rev0 ROM2B578C/2B529C; record is the native big-endian0x88-byte layout. */
void lordly_ribbon_endpoint(uint8_t *record, int32_t percent, int32_t milestone);
int32_t lordly_ribbon_stop(const uint8_t *record);
int lordly_ribbon_advance(uint8_t *record, const int16_t margins[5]);
#ifdef __cplusplus
}
#endif
#endif
