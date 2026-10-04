/* Host adaptation of accepted func_00203030.c at Decomp
 * 497181d8a6ba4bc32c35a465fccd261abe2e0c69. Explicit shifts replace the target's
 * big-endian struct alias. Arithmetic and alpha retention are unchanged. */
#include "sepia.h"
uint32_t lordly_sepia_rgba32(uint32_t pixel) {
    uint32_t weighted = (pixel >> 24) * 30 + ((pixel >> 16) & 255) * 59 +
        ((pixel >> 8) & 255) * 11;
    return (weighted * 255 / 30000) << 24 | (weighted / 150) << 16 |
        (weighted * 130 / 30000) << 8 | (pixel & 255);
}
uint16_t lordly_sepia_rgba16(uint16_t pixel) {
    uint32_t red = (pixel >> 8) & 0xf8;
    uint32_t green = (pixel >> 3) & 0xf8;
    uint32_t blue = (pixel << 2) & 0xf8;
    uint32_t weighted = red * 30 + green * 59 + blue * 11;
    red = weighted * 255 / 30000;
    green = weighted / 150;
    blue = weighted * 130 / 30000;
    return (uint16_t)(((red & 0xf8) << 8) + ((green & 0xf8) << 3) +
        ((blue & 0xf8) >> 2) | (pixel & 1));
}
