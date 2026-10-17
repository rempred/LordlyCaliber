/* Portable adaptations of accepted Decomp 17e07534 library sources:
 * sinf_000231b0.c, cosf_00020bf0.c, lookat_f_00020d40.c and
 * perspective_00022720.c. Originals remain in game/accepted/library/.
 * Preserve float/double operation order. memcpy replaces target aliasing;
 * checked outputs replace undefined conversions/division for invalid edits.
 * These are CPU math helpers, not an RSP/FCSR implementation. */
#include "graphics_math.h"
#include "scene_matrix.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Identical Rev 0 sine/cosine coefficient tables, z64 3EC90/3ED90.
 * Decimal literals round to the original doubles; the ROM reference test
 * executes both original instruction streams with those original tables. */
static const double polynomial[4] = {-0.16666659550427756, 0.008333066246082155,
                                     -0.0001980960290193795, 0.000002605780637968037};
static const double inverse_pi = 0.3183098861837907;
static const double pi_high = 3.1415926218032837, pi_low = 3.178650954705639e-8;
static unsigned exponent(float x) {
    uint32_t bits;
    memcpy(&bits, &x, sizeof(bits));
    return (bits >> 22) & 0x1ff;
}
static double series(double square) {
    return ((polynomial[3] * square + polynomial[2]) * square + polynomial[1]) * square + polynomial[0];
}
float lordly_graphics_sin(float x) {
    const unsigned exp = exponent(x);
    double y, t, value;
    int n;
    if (exp < 0xff) {
        if (exp < 0xe6)
            return x;
        y = x;
        t = y * y;
        return (float)(y + y * t * series(t));
    }
    if (exp >= 0x136)
        return isnan(x) ? NAN : 0.f;
    y = x;
    t = y * inverse_pi;
    n = t >= 0.0 ? (int)(t + 0.5) : (int)(t - 0.5);
    t = (double)n;
    y -= t * pi_high;
    y -= t * pi_low;
    t = y * y;
    value = y + y * t * series(t);
    return (n & 1) ? -(float)value : (float)value;
}
float lordly_graphics_cos(float x) {
    double y, t, value;
    int n;
    if (exponent(x) >= 0x136)
        return isnan(x) ? NAN : 0.f;
    y = x > 0.f ? x : -x;
    t = y * inverse_pi + 0.5;
    n = t >= 0.0 ? (int)(t + 0.5) : (int)(t - 0.5);
    t = (double)n;
    t -= 0.5;
    y -= t * pi_high;
    y -= t * pi_low;
    t = y * y;
    t = y * t * series(t);
    value = y + t;
    return (n & 1) ? -(float)value : (float)value;
}
static int commit(float out[16], const float matrix[16]) {
    int i;
    for (i = 0; i < 16; ++i)
        if (!isfinite(matrix[i]))
            return 0;
    memcpy(out, matrix, 16 * sizeof(float));
    return 1;
}
int lordly_graphics_look_at(float out[16], const float eye[3], const float target[3],
                            const float up[3]) {
    float m[16], lx, ly, lz, rx, ry, rz, ux, uy, uz, length;
    int i;
    for (i = 0; i < 3; ++i)
        if (!isfinite(eye[i]) || !isfinite(target[i]) || !isfinite(up[i]))
            return 0;
    lx = target[0] - eye[0]; ly = target[1] - eye[1]; lz = target[2] - eye[2];
    length = sqrtf(lx * lx + ly * ly + lz * lz);
    if (!(length > 0.f) || !isfinite(length))
        return 0;
    length = (float)(-1.0 / (double)length);
    lx *= length; ly *= length; lz *= length;
    rx = up[1] * lz - up[2] * ly;
    ry = up[2] * lx - up[0] * lz;
    rz = up[0] * ly - up[1] * lx;
    length = sqrtf(rx * rx + ry * ry + rz * rz);
    if (!(length > 0.f) || !isfinite(length))
        return 0;
    length = (float)(1.0 / (double)length);
    rx *= length; ry *= length; rz *= length;
    ux = ly * rz - lz * ry; uy = lz * rx - lx * rz; uz = lx * ry - ly * rx;
    length = sqrtf(ux * ux + uy * uy + uz * uz);
    if (!(length > 0.f) || !isfinite(length))
        return 0;
    length = (float)(1.0 / (double)length);
    ux *= length; uy *= length; uz *= length;
    lordly_matrix_identity(m);
    m[0] = rx; m[4] = ry; m[8] = rz;
    m[12] = -(eye[0] * rx + eye[1] * ry + eye[2] * rz);
    m[1] = ux; m[5] = uy; m[9] = uz;
    m[13] = -(eye[0] * ux + eye[1] * uy + eye[2] * uz);
    m[2] = lx; m[6] = ly; m[10] = lz;
    m[14] = -(eye[0] * lx + eye[1] * ly + eye[2] * lz);
    return commit(out, m);
}
int lordly_graphics_perspective(float out[16], float fov, float aspect, float near_plane,
                                float far_plane, float scale) {
    float m[16], cot;
    int i;
    if (!isfinite(fov) || !isfinite(aspect) || !isfinite(near_plane) || !isfinite(far_plane) ||
        !isfinite(scale) || fov <= 0.f || fov >= 179.f || aspect <= 0.f ||
        near_plane <= 0.f || far_plane <= near_plane || scale <= 0.f)
        return 0;
    lordly_matrix_identity(m);
    fov = (float)((double)fov * 0.017453292222222222);
    cot = lordly_graphics_cos(fov / 2.f) / lordly_graphics_sin(fov / 2.f);
    m[0] = cot / aspect; m[5] = cot;
    m[10] = (near_plane + far_plane) / (near_plane - far_plane);
    m[11] = -1.f;
    m[14] = (2.f * near_plane * far_plane) / (near_plane - far_plane);
    m[15] = 0.f;
    for (i = 0; i < 16; ++i)
        m[i] *= scale;
    return commit(out, m);
}
