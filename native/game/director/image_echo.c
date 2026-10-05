/* Host reconstruction of Rev0 ROM2B3798/2B38A0/2B38D0. Reference original
 * assembly at Decomp main62cc7ef4. Native heap pointers become three owned slots.
 * Preserve five-update sampling, counter skip while draining, low-byte operands
 * and the signed -1 query during the tail. Not accepted matching C. */
#include "image_echo.h"
#include "scene_matrix.h"
#include <math.h>
#include <string.h>
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static void put(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
static int32_t signed_word(uint32_t v) {
    return v <= 0x7fffffffU ? (int32_t)v : -1 - (int32_t)~v;
}
static float real(const uint8_t *p) {
    uint32_t v = word(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}
static void put_real(uint8_t *p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    put(p, v);
}
static int quantize(float m[16]) {
    uint8_t p[64];
    if (!lordly_matrix_pack(p, m))
        return 0;
    lordly_matrix_unpack(m, p);
    return 1;
}
int lordly_image_echo_start(uint8_t context[0x848], uint8_t channel[88], const uint32_t args[3]) {
    uint8_t next[0x848], view[88];
    float m[16], t[16], scale[16];
    memcpy(next, context, sizeof(next));
    memcpy(view, channel, sizeof(view));
    put(next + 0x828, 0);
    put(next + 0x82c, args[1] & 255);
    next[0x83a] = (uint8_t)args[2];
    next[0x83b] = (uint8_t)args[0];
    if (next[0x83b] == 1) {
        const float x = real(view + 0x40), y = real(view + 0x44), s = real(view + 0x54);
        lordly_matrix_translate(t, x, y, 0);
        lordly_matrix_scale(scale, s, s, s);
        if (!quantize(t) || !quantize(scale))
            return 0;
        lordly_matrix_multiply(t, t, scale);
        if (!quantize(t))
            return 0;
        lordly_matrix_unpack(m, next);
        lordly_matrix_multiply(m, t, m);
        if (!lordly_matrix_pack(next, m))
            return 0;
        lordly_matrix_unpack(m, next);
        for (int i = 0; i < 16; ++i)
            put_real(next + 0x7d8 + i * 4, m[i]);
        put_real(view + 0x54, 1);
        put_real(view + 0x40, 0);
        put_real(view + 0x44, 0);
    }
    memcpy(context, next, sizeof(next));
    memcpy(channel, view, sizeof(view));
    return 1;
}
uint32_t lordly_image_echo_query(const uint8_t context[0x848]) {
    const uint32_t duration = word(context + 0x82c);
    return duration == 0 ? 0 : duration == 0xffffffffU ? 0xffffffffU : 1;
}
void lordly_image_echo_fade(uint8_t c[0x848], const LordlyImageEcho *echo, uint16_t pressed) {
    const int32_t sample = signed_word(word(c + 0x824));
    if (sample > 0) {
        if (!c[0x83b]) {
            for (int32_t n = sample > 3 ? sample - 3 : 0; n < sample; ++n) {
                const int slot = n % 3;
                if (echo->live[slot] && c[0x820 + slot] > 20)
                    c[0x820 + slot] -= 20;
            }
        } else {
            const int32_t end = sample > 3 ? sample - 4 : -1;
            for (int32_t n = sample; n > end; --n) {
                const int slot = n % 3;
                if (echo->live[slot] && c[0x820 + slot] > 20)
                    c[0x820 + slot] -= 20;
            }
        }
    }
    if (c[0x840] == 1 && (pressed & 0x1000))
        c[0x840] = 2;
}
static int advance(LordlyImageEcho *echo, uint8_t *c, double settled) {
    uint32_t duration = word(c + 0x82c), elapsed = word(c + 0x828), sample = word(c + 0x824);
    float m[16];
    if (!duration)
        return 1;
    if (sample > 0x7fffffffU)
        return 0;
    elapsed++;
    put(c + 0x828, elapsed);
    if (duration == 0xffffffffU) {
        if (signed_word(elapsed) % 5 == 0 && c[0x83a]) {
            put(c + 0x828, ++elapsed);
            const uint32_t slot = sample % 3;
            echo->live[slot] = 0;
            put(c + 0x40 + slot * 4, 0);
            put(c + 0x824, ++sample);
        }
        if (signed_word(elapsed) >= signed_word(word(c + 0x830))) {
            put(c + 0x82c, 0);
            put(c + 0x824, 0);
            c[0x83c] = 4;
            for (int i = 0; i < 3; ++i) {
                echo->live[i] = 0;
                put(c + 0x40 + i * 4, 0);
            }
        }
        return 1;
    }
    if (elapsed == duration) {
        put(c + 0x82c, 0xffffffffU);
        put(c + 0x830, elapsed + c[0x83a] * 5U);
        if (!c[0x83b]) {
            const float x = real(c + 0x818), y = real(c + 0x81c);
            if (x == 0 || y == 0 || !isfinite(x) || !isfinite(y) || !isfinite(settled))
                return 0;
            lordly_matrix_scale(m, (float)(settled / x), (float)(settled / y), 1);
        } else
            for (int i = 0; i < 16; ++i)
                m[i] = real(c + 0x798 + i * 4);
    } else {
        const float e = (float)signed_word(elapsed), d = (float)signed_word(duration);
        const float ratio = c[0x83b] ? e / d : (d - e) / d;
        for (int i = 0; i < 16; ++i) {
            const float a = real(c + 0x798 + i * 4), b = real(c + 0x7d8 + i * 4);
            m[i] = (a - b) * ratio + b;
        }
    }
    if (!lordly_matrix_pack(c, m))
        return 0;
    if (signed_word(elapsed) % 5 == 0 && c[0x83a]) {
        const uint32_t slot = sample % 3;
        memcpy(echo->matrix[slot], c, 64);
        echo->live[slot] = 1;
        put(c + 0x40 + slot * 4, slot + 1);
        duration = word(c + 0x82c);
        const uint32_t progress = c[0x83b] ? duration - elapsed : elapsed;
        const int32_t numerator = signed_word((255U - c[0x83d]) * progress);
        const int32_t divisor = signed_word(duration);
        if (!divisor || (numerator == (-2147483647 - 1) && divisor == -1))
            return 0;
        c[0x820 + slot] = (uint8_t)((uint32_t)(numerator / divisor) + c[0x83d]);
        put(c + 0x824, sample + 1);
    }
    return 1;
}
int lordly_image_echo_advance(LordlyImageEcho *echo, uint8_t context[0x848], double settled) {
    uint8_t next[0x848];
    LordlyImageEcho owned = *echo;
    memcpy(next, context, sizeof(next));
    if (!advance(&owned, next, settled))
        return 0;
    *echo = owned;
    memcpy(context, next, sizeof(next));
    return 1;
}
