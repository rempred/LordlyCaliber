#ifndef LORDLY_IMAGE_PRESENTATION_H
#define LORDLY_IMAGE_PRESENTATION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* RGBA32 storage width is rounded to four pixels. RGB is retained. */
int lordly_image_vignette(uint8_t *rgba, int width, int height, int alpha_cap);
/* Original 3A constructor: packed current matrix and floating endpoints. */
int lordly_image_matrices(uint8_t context[0x848], const float camera[14], const float placement[4],
                          uint32_t flags, const double constants[5], float radians);
#ifdef __cplusplus
}
#endif
#endif
