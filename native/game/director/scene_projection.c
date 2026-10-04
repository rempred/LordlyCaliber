#include "scene_projection.h"
/* Start adapts accepted func_002A91E4.c at Decomp commit
 * f53a12e53f3f6e7e389cd6e011b5be25d31bde15. Init/update are portable
 * reconstructions of ROM2A90EC and ROM2A9258, not matching-C claims. */
#include <math.h>
#include <string.h>
static float read_float(const LordlySceneProjection *p, int at) {
    const uint8_t *b = p->bytes + at;
    uint32_t word = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    float value; memcpy(&value, &word, 4); return value;
}
static void write_float(LordlySceneProjection *p, int at, float value) {
    uint32_t word; uint8_t *b = p->bytes + at;
    memcpy(&word, &value, 4);
    b[0] = (uint8_t)(word >> 24); b[1] = (uint8_t)(word >> 16);
    b[2] = (uint8_t)(word >> 8); b[3] = (uint8_t)word;
}
void lordly_projection_initialize(LordlySceneProjection *p) {
    int i;
    if (!p) return;
    for (i = 0; i < 9; ++i) {
        float v = i < 3 ? 1.f : i == 5 ? -50.f : 0.f;
        write_float(p, i * 4, v); write_float(p, 0x24 + i * 4, v);
    }
    p->bytes[0x48] = p->bytes[0x49] = 0;
}
int lordly_projection_start(LordlySceneProjection *p, float x, float y, float sx, float sy, int32_t duration) {
    static const int at[4] = {0xc, 0x10, 0, 4};
    float target[4]; int i; LordlySceneProjection next;
    if (!p || !duration) return 1;
    target[0] = x; target[1] = y; target[2] = sx; target[3] = sy; next = *p;
    for (i = 0; i < 4; ++i) {
        float delta;
        if (!isfinite(target[i])) return 1;
        delta = (target[i] - read_float(p, at[i] + 0x24)) / (float)duration;
        if (!isfinite(delta)) return 1;
        write_float(&next, at[i], delta);
    }
    next.bytes[0x48] = (uint8_t)((uint32_t)duration >> 8);
    next.bytes[0x49] = (uint8_t)duration;
    *p = next; return 0;
}
int lordly_projection_advance(LordlySceneProjection *p) {
    static const int at[4] = {0xc, 0, 0x10, 4};
    int count, i; LordlySceneProjection next;
    if (!p) return 0;
    count = p->bytes[0x48] * 256 + p->bytes[0x49];
    if (!count) return 0;
    next = *p;
    for (i = 0; i < 4; ++i) {
        float value = read_float(p, at[i] + 0x24) + read_float(p, at[i]);
        if (!isfinite(value)) return 1;
        write_float(&next, at[i] + 0x24, value);
    }
    --count; next.bytes[0x48] = (uint8_t)(count >> 8); next.bytes[0x49] = (uint8_t)count;
    *p = next; return 0;
}
