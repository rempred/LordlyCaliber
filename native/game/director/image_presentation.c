/* PC reconstruction of Rev0 ROM2B25DC/2B24F0 and2B3300/2B3494.
 * Original instructions in Decomp main62cc7ef4; not accepted matching C.
 * Retain single/double arithmetic and packed-matrix intermediate steps.
 * Host libm supplies sqrt/atan2/sin/cos, as in the other desktop matrix owners. */
#include "image_presentation.h"
#include "scene_matrix.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static float real(const uint8_t *p) {
    uint32_t bits = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static void put_real(uint8_t *p, float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    p[0] = (uint8_t)(bits >> 24);
    p[1] = (uint8_t)(bits >> 16);
    p[2] = (uint8_t)(bits >> 8);
    p[3] = (uint8_t)bits;
}
int lordly_image_vignette(uint8_t *rgba, int width, int height, int alpha_cap) {
    int x, y;
    if (!rgba || width < 4 || width > 2048 || width % 4 || height < 2 || height > 2048 ||
        alpha_cap < 0 || alpha_cap > 255)
        return 0;
    const int rx = width / 2, ry = (int)((float)height * .58f);
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x) {
            const int px = x - width / 2, py = y - height / 2;
            int alpha = 400;
            if (px || py) {
                const float slope = px ? (py ? (float)py / (float)px : 0.f) : 10000.f;
                float term = (float)(rx * rx) * slope;
                term = term * slope;
                const float intercept = (float)(rx * ry) / sqrtf((float)(ry * ry) + term);
                const float edge_y = intercept * slope;
                const float radius = edge_y * edge_y + intercept * intercept;
                const float remaining = radius - (float)(py * py + px * px);
                alpha = remaining < 0.f ? 0 : (int)((remaining / radius) * 400.f);
            }
            if (alpha > alpha_cap)
                alpha = alpha_cap;
            /* The original searches sixteen distances, but writes only when the
             * first strict boundary comparison occurs below eight. */
            for (int distance = 0; distance < 16; ++distance) {
                if (height / 2 - distance < abs(py) || width / 2 - distance < abs(px)) {
                    if (distance < 8 && alpha > distance * 16)
                        alpha = distance * 16;
                    break;
                }
            }
            rgba[(y * width + x) * 4 + 3] = (uint8_t)alpha;
        }
    return 1;
}
static int quantize(float m[16]) {
    uint8_t packed[64];
    if (!lordly_matrix_pack(packed, m))
        return 0;
    lordly_matrix_unpack(m, packed);
    return 1;
}
int lordly_image_matrices(uint8_t context[0x848], const float camera[14], const float placement[4],
                          uint32_t flags, const double constants[5], float radians) {
    float m[16], other[16], angle[3], end[16];
    uint8_t packed[64];
    int i;
    const float sx = real(context + 0x818), sy = real(context + 0x81c);
    for (i = 0; i < 14; ++i)
        if (!isfinite(camera[i]))
            return 0;
    for (i = 0; i < 4; ++i)
        if (!isfinite(placement[i]))
            return 0;
    for (i = 0; i < 5; ++i)
        if (!isfinite(constants[i]))
            return 0;
    if (!isfinite(sx) || !isfinite(sy) || sx == 0 || sy == 0 || constants[1] == 0 ||
        constants[3] == 0 || !isfinite(radians))
        return 0;
    const float dx = camera[5] - camera[8], dy = camera[6] - camera[9], dz = camera[7] - camera[10];
    const float xy = sqrtf(dy * dy + dx * dx), zy = sqrtf(dz * dz + dy * dy),
                xz = sqrtf(dx * dx + dz * dz);
    angle[0] = (float)((double)atan2f(xy, dz) * constants[0] / constants[1] + constants[2]);
    angle[1] = (float)((double)atan2f(zy, dx) * constants[0] / constants[1] + constants[2]);
    angle[2] = (float)((double)atan2f(xz, dy) * constants[0] / constants[1] + constants[2]);
    lordly_matrix_rotation(m, angle[0], angle[1], angle[2], radians);
    lordly_matrix_rotation(other, 0.f, flags & 8 ? 0.f : -90.f, flags & 8 ? -5.f : 5.f, radians);
    if (!quantize(m) || !quantize(other))
        return 0;
    lordly_matrix_multiply(m, other, m);
    if (!quantize(m))
        return 0;
    lordly_matrix_scale(other, (float)((double)placement[2] / constants[3] / sx),
                        (float)((double)placement[3] / constants[3] / sy), 1.f);
    if (!quantize(other))
        return 0;
    lordly_matrix_multiply(m, other, m);
    if (!quantize(m))
        return 0;
    lordly_matrix_translate(other, placement[0], placement[1], 0.f);
    if (!quantize(other))
        return 0;
    lordly_matrix_multiply(m, m, other);
    if (!lordly_matrix_pack(packed, m))
        return 0;
    lordly_matrix_unpack(m, packed);
    lordly_matrix_scale(end, (float)(constants[4] / sx), (float)(constants[4] / sy), 1.f);
    for (i = 0; i < 16; ++i)
        if (!isfinite(end[i]))
            return 0;
    memcpy(context, packed, 64);
    for (i = 0; i < 16; ++i) {
        put_real(context + 0x798 + i * 4, m[i]);
        put_real(context + 0x7d8 + i * 4, end[i]);
    }
    return 1;
}
