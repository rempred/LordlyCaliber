/* Portable reconstruction of original ROM 29B78C..29B9E0 and
 * 29BA80..29BDC4. Original assembly at Decomp 62cc7ef4 is the reference;
 * these functions are not an accepted matching-C translation. */
#include "actor_material.h"
#include <math.h>
static int half(const uint8_t *p) {
    unsigned v = (unsigned)p[0] * 256 + p[1];
    return v < 32768 ? (int)v : (int)v - 65536;
}
static void put_half(uint8_t *p, unsigned v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
static uint32_t environment(const uint8_t rgb[3]) {
    return (uint32_t)rgb[0] << 24 | (uint32_t)rgb[1] << 16 | (uint32_t)rgb[2] << 8 | 1;
}
void lordly_tint_begin(uint8_t r[12], const uint32_t a[8]) {
    int i;
    for (i = 0; i < 6; ++i)
        r[i] = (uint8_t)a[i];
    put_half(r + 6, a[7]);
    put_half(r + 8, a[6]);
    put_half(r + 10, 0);
}
void lordly_material_begin(uint8_t r[7], const uint32_t a[7]) {
    int i;
    r[0] = (uint8_t)a[6];
    for (i = 0; i < 6; ++i)
        r[1 + i] = (uint8_t)a[i];
}
int lordly_tint_render(uint8_t *r, uint8_t rgb[3], uint32_t commands[6]) {
    int i, progress, duration;
    uint8_t next[3];
    commands[0] = 0xe7000000U;
    commands[1] = 0;
    if (!r)
        return 2;
    progress = half(r + 10);
    duration = half(r + 8);
    if (progress >= duration) {
        progress = duration;
        for (i = 0; i < 3; ++i)
            next[i] = r[3 + i];
    } else {
        float fraction;
        if (!duration)
            return -1;
        ++progress;
        fraction = (float)progress / (float)duration;
        for (i = 0; i < 3; ++i) {
            float delta = fraction * (float)((int)r[3 + i] - r[i]);
            if (!isfinite(delta) || delta < -2147483648.0 || delta >= 2147483648.0)
                return -1;
            next[i] = (uint8_t)(r[i] + (int32_t)delta);
        }
    }
    put_half(r + 10, (unsigned)progress);
    for (i = 0; i < 3; ++i)
        rgb[i] = next[i];
    if (!half(r + 6))
        return 2;
    commands[2] = 0xfc62fec3U;
    commands[3] = 0x200d723dU;
    commands[4] = 0xfb000000U;
    commands[5] = environment(rgb);
    return 6;
}
int lordly_material_render(const uint8_t *r, uint8_t rgb[3], uint32_t commands[14]) {
    int i;
    if (!r)
        return 0;
    commands[0] = commands[2] = commands[6] = 0xe7000000U;
    commands[1] = commands[3] = commands[7] = 0;
    commands[4] = 0xe3000a01U;
    commands[5] = 0x00100000U;
    commands[8] = 0xe200001cU;
    commands[9] = 0x00104240U;
    commands[10] = r[0] == 1 ? 0xfc629603U : 0xfc2296c3U;
    commands[11] = r[0] == 1 ? 0x2f2d7bfdU : 0xf22ffa3dU;
    commands[12] = 0xfb000000U;
    commands[13] = environment(r + 1);
    for (i = 0; i < 3; ++i)
        rgb[i] = r[4 + i];
    return 14;
}
