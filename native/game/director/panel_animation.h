#ifndef LORDLY_PANEL_ANIMATION_H
#define LORDLY_PANEL_ANIMATION_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Reconstructed shared entity opening/closing, ROM51EB0/51F74/52284/5A0F4.
 * Live list ownership is supplied by the host. Returns1 when detached. */
void lordly_panel_initialize(uint8_t record[232], uint16_t kind, uint32_t callback,
                             const int32_t initial[4], const int32_t target[4]);
void lordly_panel_close(uint8_t record[232]);
int lordly_panel_animate(uint8_t record[232]);
#ifdef __cplusplus
}
#endif
#endif
