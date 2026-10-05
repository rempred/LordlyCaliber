#include "path_curve.h"
#include <math.h>
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static void set32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static int coefficients(const float *t, const float *v, float *c, int n) {
    double h[20], g[20];
    c[0] = c[n - 1] = 0;
    for (int i = 0; i < n - 1; ++i) {
        h[i] = (float)(t[i + 1] - t[i]);
        if (!(h[i] > 0))
            return 0;
        g[i] = (double)(float)(v[i + 1] - v[i]) / h[i];
    }
    c[1] = (float)(g[1] - g[0] - h[0] * (double)c[0]);
    float span = t[2] - t[0];
    g[0] = (float)(span + span);
    for (int i = 1; i < n - 2; ++i) {
        double factor = h[i] / g[i - 1];
        c[i + 1] = (float)(g[i + 1] - g[i] - (double)c[i] * factor);
        span = t[i + 2] - t[i];
        g[i] = (double)(float)(span + span) - h[i] * factor;
    }
    c[n - 2] = (float)((double)c[n - 2] - h[n - 2] * (double)c[n - 1]);
    for (int i = n - 2; i > 0; --i)
        c[i] = (float)(((double)c[i] - h[i] * (double)c[i + 1]) / g[i - 1]);
    return 1;
}
static float evaluate(float at, const float *t, const float *v, const float *c, int n) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (t[mid] < at)
            lo = mid + 1;
        else
            hi = mid;
    }
    int i = lo - (lo > 0);
    float d = at - t[i], h = t[i + 1] - t[i];
    float a = c[i + 1] - c[i];
    a = a * d;
    a = a / h;
    float b = v[i + 1] - v[i];
    b = b / h;
    float q = c[i] + c[i];
    q = q + c[i + 1];
    q = q * h;
    float triple = c[i] * 3.f;
    a = a + triple;
    a = a * d;
    b = b - q;
    a = a + b;
    a = a * d;
    return a + v[i];
}
int lordly_path_sample(float *out_x, float *out_y, int samples, int points, const float *x,
                       const float *y, double time_scale) {
    if (points < 2 || points > 20 || samples < 2 || samples > 16384 || !isfinite(time_scale))
        return 0;
    for (int i = 0; i < points; ++i)
        if (!isfinite(x[i]) || !isfinite(y[i]))
            return 0;
    if (points == 2) {
        float dx = (x[1] - x[0]) / (float)samples, dy = (y[1] - y[0]) / (float)samples;
        out_x[0] = x[0];
        out_y[0] = y[0];
        for (int i = 1; i < samples; ++i) {
            out_x[i] = out_x[i - 1] + dx;
            out_y[i] = out_y[i - 1] + dy;
        }
        return 1;
    }
    float t[20], cx[20], cy[20];
    t[0] = 0;
    for (int i = 1; i < points; ++i) {
        float dx = x[i] - x[i - 1], dy = y[i] - y[i - 1];
        float a = dx * dx, b = dy * dy;
        t[i] = t[i - 1] + sqrtf(a + b);
    }
    if (!(t[points - 1] > 0) || !isfinite(t[points - 1]))
        return 0;
    for (int i = 1; i < points; ++i)
        t[i] = t[i] / t[points - 1];
    if (!coefficients(t, x, cx, points) || !coefficients(t, y, cy, points))
        return 0;
    for (int i = 0; i < samples; ++i) {
        float at = (float)((time_scale / (double)samples) * (double)i);
        out_x[i] = evaluate(at, t, x, cx, points);
        out_y[i] = evaluate(at, t, y, cy, points);
        if (!isfinite(out_x[i]) || !isfinite(out_y[i]))
            return 0;
    }
    return 1;
}
void lordly_ribbon_endpoint(uint8_t *r, int32_t percent, int32_t milestone) {
    if (milestone != -1)
        set32(r + 0x54, (uint32_t)milestone);
    else if (percent != -1)
        set32(r + 0x50, (uint32_t)percent);
}
int32_t lordly_ribbon_stop(const uint8_t *r) {
    int32_t milestone = (int32_t)get32(r + 0x54);
    uint32_t stop = get32(r + 0x58) * 2 - 2;
    if (milestone != -1)
        stop = get32(r + 0x10 + 4 * milestone) * 2 - 2;
    else if (get32(r + 0x50) != UINT32_MAX)
        return (int32_t)(stop * get32(r + 0x50)) / 100;
    return (int32_t)stop;
}
int lordly_ribbon_advance(uint8_t *r, const int16_t margins[5]) {
    uint16_t phase = (uint16_t)(((uint16_t)r[0x80] << 8 | r[0x81]) + 2);
    if ((int16_t)phase > 64)
        phase = 0;
    r[0x80] = (uint8_t)(phase >> 8);
    r[0x81] = (uint8_t)phase;
    int32_t cursor = (int32_t)get32(r + 0x68);
    int32_t edge =
        (int32_t)((uint32_t)cursor * 2 + (uint32_t)(int32_t)margins[(uint8_t)(r[0x82] - 1)] * 2);
    if (edge < lordly_ribbon_stop(r))
        set32(r + 0x68, (uint32_t)cursor + 2);
    else
        r[0x85] = 0;
    if (r[0x86]) {
        r[0x86]++;
        if (r[0x86] == 30)
            return 0;
    }
    return 1;
}
