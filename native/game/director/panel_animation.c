#include "panel_animation.h"
#include <stdlib.h>
#include <string.h>
static uint16_t half(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static void put16(uint8_t *p, uint32_t n) {
    p[0] = (uint8_t)(n >> 8);
    p[1] = (uint8_t)n;
}
static void put32(uint8_t *p, uint32_t n) {
    p[0] = (uint8_t)(n >> 24);
    p[1] = (uint8_t)(n >> 16);
    p[2] = (uint8_t)(n >> 8);
    p[3] = (uint8_t)n;
}
static float real(const uint8_t *p) {
    const uint32_t n = word(p);
    float f;
    memcpy(&f, &n, 4);
    return f;
}
static void put_real(uint8_t *p, float f) {
    uint32_t n;
    memcpy(&n, &f, 4);
    put32(p, n);
}
static void geometry(uint8_t *r) {
    const int32_t left = (int32_t)word(r + 0x2c), top = (int32_t)word(r + 0x30),
                  right = (int32_t)word(r + 0x34), bottom = (int32_t)word(r + 0x38);
    const int32_t width = abs((int16_t)right - (int16_t)left) - 1,
                  height = abs((int16_t)bottom - (int16_t)top) - 1;
    const int32_t x[4] = {160 - right, 160 - right, 160 - left, 160 - left},
                  y[4] = {120 - bottom, 120 - top, 120 - top, 120 - bottom};
    for (int i = 0; i < 4; ++i) {
        uint8_t *v = r + 0x90 + i * 16;
        put16(v, (uint32_t)x[i]);
        put16(v + 2, (uint32_t)y[i]);
        put16(v + 8, i >= 2 ? (uint32_t)width * 64 : 0);
        put16(v + 10, i == 0 || i == 3 ? (uint32_t)height * 64 : 0);
    }
}
void lordly_panel_initialize(uint8_t r[232], uint16_t kind, uint32_t callback,
                             const int32_t initial[4], const int32_t target[4]) {
    memset(r, 0, 232);
    put32(r + 8, callback);
    r[0x14] = 0x88;
    put16(r + 0x16, kind);
    for (int i = 0; i < 4; ++i) {
        put32(r + 0x2c + i * 4, (uint32_t)initial[i]);
        put32(r + 0x4c + i * 4, (uint32_t)initial[i]);
        put32(r + 0x3c + i * 4, (uint32_t)target[i]);
    }
    const uint8_t colors[4] = {0xb5, 0xd5, 0xff, 0xe0};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j)
            r[0x9c + i * 16 + j] = colors[i];
        r[0x9f + i * 16] = 255;
    }
    put16(r + 0x26, 0x8000);
    put16(r + 0x28, 1);
    geometry(r);
}
void lordly_panel_close(uint8_t r[232]) {
    int same = 1;
    for (int i = 0; i < 4; ++i)
        if (word(r + 0x2c + i * 4) != word(r + 0x4c + i * 4))
            same = 0;
    if (same) {
        const int32_t center[2] = {160, 120};
        for (int i = 0; i < 2; ++i) {
            const int32_t a = (int32_t)word(r + 0x2c + i * 4), b = (int32_t)word(r + 0x34 + i * 4);
            const int left = abs(b - center[i]) < abs(a - center[i]);
            put32(r + 0x3c + i * 4, (uint32_t)(left ? a : b - 8));
            put32(r + 0x44 + i * 4, (uint32_t)(left ? a + 8 : b));
        }
    } else
        for (int i = 0; i < 4; ++i)
            put32(r + 0x3c + i * 4, word(r + 0x4c + i * 4));
    put16(r + 0x28, 2);
    put16(r + 0x26, 0x8000);
}
int lordly_panel_animate(uint8_t r[232]) {
    const int phase = half(r + 0x28);
    uint16_t flags = half(r + 0x26);
    if (phase == 1 || phase == 2) {
        if (flags & 0x8000) {
            for (int i = 0; i < 4; ++i) {
                const int32_t a = (int32_t)word(r + 0x2c + i * 4),
                              b = (int32_t)word(r + 0x3c + i * 4);
                put_real(r + 0x5c + i * 4, (float)a);
                put_real(r + 0x6c + i * 4, (float)(b - a) / 8.f);
            }
            put16(r + 0x18, phase == 1 ? 0 : 255);
            put16(r + 0x1c, 32);
            put16(r + 0x1e, 8);
            put16(r + 0x20, 8);
            if (phase == 1)
                flags |= 1;
        }
        int fade = 0;
        if (phase == 1) {
            if (half(r + 0x1e) < 4) {
                fade = 1;
                if (!half(r + 0x20)) {
                    put16(r + 0x18, 255);
                    put16(r + 0x28, 0);
                } else {
                    int n = (int16_t)half(r + 0x18) + (int16_t)half(r + 0x1c);
                    put16(r + 0x18, n > 255 ? 255 : (uint32_t)n);
                }
            }
        } else {
            fade = 1;
            if (half(r + 0x20) == 4)
                flags |= 0x4001;
            const int n = (int16_t)half(r + 0x18) - (int16_t)half(r + 0x1c);
            put16(r + 0x18, n < 0 ? 0 : (uint32_t)n);
        }
        if (fade)
            put16(r + 0x20, half(r + 0x20) - 1U);
    }
    if (flags) {
        if (flags & 1) {
            for (int i = 0; i < 4; ++i)
                if (half(r + 0x1e)) {
                    const float f = real(r + 0x5c + i * 4) + real(r + 0x6c + i * 4);
                    put_real(r + 0x5c + i * 4, f);
                    put32(r + 0x2c + i * 4, (uint32_t)(int32_t)f);
                } else
                    put32(r + 0x2c + i * 4, word(r + 0x3c + i * 4));
            if (!half(r + 0x1e))
                flags &= 0xfffe;
            geometry(r);
        }
        flags &= 0x7fff;
        put16(r + 0x26, flags);
        if (half(r + 0x1e))
            put16(r + 0x1e, half(r + 0x1e) - 1U);
    }
    return flags == 0x4000;
}
