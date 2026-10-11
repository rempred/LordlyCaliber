/* Portable reconstruction of ROM 2AF470..2AF6C0, using original assembly at
 * Decomp 62cc7ef4. Preserve native axis-special cases and single-precision order. */
#include "actor_divider.h"
#include <string.h>
static int half(uint32_t v) {
    v &= 65535;
    return v < 32768 ? (int)v : (int)v - 65536;
}
static void put_real(uint8_t *p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static float real(const uint8_t *p) {
    uint32_t v = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    float f;
    memcpy(&f, &v, 4);
    return f;
}
void lordly_actor_divider_begin(uint8_t r[12], const uint32_t args[6]) {
    const int x = half(args[0]), z = half(args[1]);
    const float dx = (float)(half(args[2]) - x), dz = (float)(half(args[3]) - z);
    float a, b;
    if (dz == 0) {
        a = (float)z;
        b = 0;
    } else if (dx == 0) {
        a = 0;
        b = (float)x;
    } else {
        a = dz / dx;
        b = -(a * (float)x) + (float)z;
    }
    put_real(r, a);
    put_real(r + 4, b);
    r[8] = (uint8_t)args[4];
    r[9] = (uint8_t)args[5];
}
uint8_t lordly_actor_divider_channel(const uint8_t r[12], float x, float z) {
    const float a = real(r), b = real(r + 4);
    int selected;
    if (b == 0)
        selected = a < x;
    else if (a == 0)
        selected = b < z;
    else
        selected = a * x + b < z;
    return r[selected ? 8 : 9];
}
