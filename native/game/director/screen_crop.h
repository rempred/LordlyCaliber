#ifndef LORDLY_SCREEN_CROP_H
#define LORDLY_SCREEN_CROP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void lordly_crop_start(uint8_t record[14], const uint32_t args[6]);
int lordly_crop_active(const uint8_t record[14]);
/* ROM2AD7B0: one draw, including progress advance. Zero requests release.
 * Outputs the exact two scissor words; coverage belongs to the renderer. */
int lordly_crop_draw(uint8_t record[14], uint32_t scissor[2]);
#ifdef __cplusplus
}
#endif
#endif
