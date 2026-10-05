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

/* Host adaptation of accepted 00202198/00202258 (Decomp main62cc7ef4).
 * Body metadata substitutions apply this after optional sepia conversion. */
uint32_t lordly_warm_rgba32(uint32_t color) {
    uint32_t maximum = color >> 24, green = (color >> 16) & 255;
    uint32_t blue = (color >> 8) & 255, red;
    if (maximum < green) maximum = green;
    if (maximum < blue) maximum = blue;
    if (maximum < 128) {
        red = maximum * 112 / 128 + 24;
        green = maximum * 112 / 128 + 28;
        blue = maximum * 96 / 128 + 8;
    } else {
        red = maximum * 80 / 128 + 56;
        green = maximum * 80 / 128 + 60;
        blue = maximum * 80 / 128 + 40;
    }
    return (red << 24) | (green << 16) | (blue << 8) | (color & 255);
}
uint16_t lordly_warm_rgba16(uint16_t color) {
    uint32_t expanded = ((uint32_t)(color & 0xf800) << 16) |
                        ((uint32_t)(color & 0x07c0) << 13) |
                        ((uint32_t)(color & 0x003e) << 10);
    uint32_t warm = lordly_warm_rgba32(expanded);
    return (uint16_t)(((warm >> 16) & 0xf800) | ((warm >> 13) & 0x07c0) |
                      ((warm >> 10) & 0x003e) | (color & 1));
}
