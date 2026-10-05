#ifndef LORDLY_IMAGE_ECHO_H
#define LORDLY_IMAGE_ECHO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct LordlyImageEcho {
    uint8_t matrix[3][64];
    uint8_t live[3];
} LordlyImageEcho;
int lordly_image_echo_start(uint8_t context[0x848], uint8_t channel[88], const uint32_t args[3]);
int lordly_image_echo_advance(LordlyImageEcho *, uint8_t context[0x848], double settled_scale);
uint32_t lordly_image_echo_query(const uint8_t context[0x848]);
/* Image portion of2B529C; its separate path-ribbon pool is owned by the caller. */
void lordly_image_echo_fade(uint8_t context[0x848], const LordlyImageEcho *, uint16_t pressed);
#ifdef __cplusplus
}
#endif
#endif
