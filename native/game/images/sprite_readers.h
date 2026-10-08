#ifndef LORDLY_SPRITE_READERS_H
#define LORDLY_SPRITE_READERS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t discriminator, flags;
    uint16_t width, height;
    /* SIZE_MAX means no stored lane. */
    size_t color, alpha, palette;
} LordlySpriteChild;
typedef struct {
    unsigned width, height, count, color_format, alpha_format;
    unsigned color_stride, alpha_stride;
    LordlySpriteChild children[255];
} LordlySpriteLayout;
/* Bounded bulk adaptation of accepted func_00203414. Zero means success. */
int lordly_sprite_layout(const uint8_t *source, size_t size, LordlySpriteLayout *layout);
/* Accepted func_00203664's delta arithmetic, on explicit big-endian lanes.
 * Destination may alias delta, as it does during editor rebuilds. */
int lordly_sprite_delta(uint8_t *destination, const uint8_t *previous,
    const uint8_t *delta, size_t size, unsigned format);
#ifdef __cplusplus
}
#endif
#endif
