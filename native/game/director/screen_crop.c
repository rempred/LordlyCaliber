/* Portable reconstruction of ROM2AD6D0..2ADA48 and interpolation283D94.
 * Complete functions match retained main-scene RAM8023DF70..8023E2E8;
 * the shared interpolation helper matches RAM80227DC4. */
#include "screen_crop.h"
static int16_t half(const uint8_t *p) {
    return (int16_t)((uint16_t)p[0] << 8 | p[1]);
}
static void put(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
void lordly_crop_start(uint8_t r[14], const uint32_t a[6]) {
    put(r, a[2]);
    put(r + 2, a[3]);
    /* The preset compares full words before the halfword stores. */
    const int preset = a[0] == UINT32_MAX && a[1] == UINT32_MAX;
    put(r + 4, preset ? 24 : a[0]);
    put(r + 6, preset ? 24 : a[1]);
    put(r + 8, 0);
    put(r + 10, a[4]);
    put(r + 12, a[5]);
}
int lordly_crop_active(const uint8_t r[14]) {
    return half(r + 8) != half(r + 10);
}
int lordly_crop_draw(uint8_t r[14], uint32_t scissor[2]) {
    int32_t top = half(r), bottom = half(r + 2);
    const int32_t duration = half(r + 10);
    int32_t progress = half(r + 8);
    if (progress == duration) {
        if (!half(r + 12))
            return 0;
    } else {
        progress = (int16_t)(progress + 1);
        put(r + 8, (uint32_t)progress);
        top = half(r + 4) + (top - half(r + 4)) * progress / duration;
        bottom = half(r + 6) + (bottom - half(r + 6)) * progress / duration;
    }
    scissor[0] = 0xed000000U | (top < 0 ? 0 : ((uint32_t)top * 4 & 0xfff));
    bottom = 239 - bottom;
    scissor[1] = 0x004fc000U | (bottom == 240 ? 0x3bc : ((uint32_t)bottom * 4 & 0xfff));
    return 1;
}
