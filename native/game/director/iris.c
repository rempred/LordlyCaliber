#include "iris.h"
#include <math.h>
#include <string.h>
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static int16_t half(const uint8_t *p) {
    return (int16_t)((uint16_t)p[0] << 8 | p[1]);
}
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static void put16(uint8_t *p, int32_t v) {
    p[0] = (uint8_t)((uint32_t)v >> 8);
    p[1] = (uint8_t)v;
}
int lordly_iris_create(uint8_t r[24], const uint32_t args[9], int count) {
    const int32_t duration = (int32_t)args[6];
    if (count < 2 || count > 20 || duration < 1 || duration > 30000 || args[7] > 1)
        return 0;
    int32_t a[6];
    for (int i = 0; i < 6; ++i)
        a[i] = (int16_t)args[i];
    if (a[0] < 0 || a[1] < 0 || a[5] < 0 || a[5] > 255)
        return 0;
    for (int i = 0; i < 4; ++i) {
        int64_t product = (int64_t)a[i] * duration;
        if (product > INT32_MAX || product < INT32_MIN)
            return 0;
    }
    if (fmax(a[0] * 1.3, 640) + fabs((double)a[2]) > 32767 ||
        fmax(a[1] * 1.3, 480) + fabs((double)a[3]) > 32767)
        return 0;
    memset(r, 0, 24);
    put32(r, args[7] ? args[6] : 0);
    put32(r + 4, args[6]);
    for (int i = 0; i < 6; ++i)
        put16(r + 8 + i * 2, a[i]);
    r[20] = (uint8_t)args[7];
    r[21] = (uint8_t)(count - 2);
    return 1;
}
int lordly_iris_query(const uint8_t r[24]) {
    return (int32_t)word(r) <= (int32_t)word(r + 4);
}
int lordly_iris_advance(uint8_t r[24]) {
    int32_t p = (int32_t)word(r), d = (int32_t)word(r + 4);
    if (!r[20]) {
        if (p > d)
            return 0;
        put32(r, (uint32_t)p + 1);
        if (p == d) {
            r[21] = 0;
            return 1;
        }
    } else {
        if (!p)
            return 2;
        put32(r, (uint32_t)p - 1);
    }
    return 0;
}
int lordly_iris_geometry(const uint8_t r[24], int32_t terms[5]) {
    const int32_t p = (int32_t)word(r), d = (int32_t)word(r + 4);
    terms[4] = half(r + 18);
    if (p >= d)
        return 1;
    terms[0] = (int32_t)((int64_t)half(r + 8) * (d - p) / d);
    terms[1] = (int32_t)((int64_t)half(r + 10) * (d - p) / d);
    terms[2] = (int32_t)((int64_t)half(r + 12) * p / d);
    terms[3] = (int32_t)((int64_t)half(r + 14) * p / d);
    return 0;
}
static void vertex(uint8_t *v, int32_t x, int32_t y, int32_t alpha) {
    memset(v, 0, 16);
    put16(v, x);
    put16(v + 2, y);
    v[15] = (uint8_t)alpha;
}
int lordly_iris_mesh(uint8_t vertices[1488], const int32_t t[5], int quad, const double k[3]) {
    if (quad) {
        vertex(vertices, -320, -240, t[4]);
        vertex(vertices + 16, -320, 240, t[4]);
        vertex(vertices + 32, 320, 240, t[4]);
        vertex(vertices + 48, 320, -240, t[4]);
        return 4;
    }
    for (int degrees = 0, index = 0; degrees <= 360; degrees += 12, index += 3) {
        const float angle = (float)((double)degrees * k[1] / k[2]);
        const float cosine = cosf(angle), sine = sinf(angle);
        const float x = (float)(int16_t)t[0] * cosine, y = (float)(int16_t)t[1] * sine;
        float ox = cosine * 320.f, oy = sine * 240.f;
        ox = ox + ox;
        oy = oy + oy;
        vertex(vertices + index * 16, (int32_t)x + t[2], (int32_t)y + t[3], 0);
        vertex(vertices + (index + 1) * 16, (int32_t)((double)x * k[0]) + t[2],
               (int32_t)((double)y * k[0]) + t[3], t[4]);
        vertex(vertices + (index + 2) * 16, (int32_t)ox + t[2], (int32_t)oy + t[3], t[4]);
    }
    return 93;
}
