/* Portable reconstruction of ROM 2AF78C, 2B118C and 2B151C from original ASM
 * at Decomp 62cc7ef4. The completion predicate adapts accepted func_002B1014.c,
 * replacing the scene-root pointer with explicit host job ownership. */
#include "scene_transform.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static const int fields[6] = {0x48, 0x4c, 0x40, 0x44, 0x50, 0x54};
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static int32_t signed_word(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : -1 - (int32_t)~v;
}
static void put_word(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
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
    put_word(p, v);
}
void lordly_transform_identity(uint8_t records[20][88]) {
    int channel;
    memset(records, 0, 20 * 88);
    for (channel = 0; channel < 20; ++channel) {
        put_word(records[channel], 0x00010000);
        put_word(records[channel] + 8, 1);
        put_word(records[channel] + 20, 0x00010000);
        put_word(records[channel] + 28, 1);
        put_real(records[channel] + 0x54, 1.f);
    }
}
int lordly_transform_prepare(uint8_t records[20][88], float delta[20][6],
                             const uint8_t current[480], const uint8_t *target, int32_t duration) {
    float values[20][6], steps[20][6];
    int channel, field;
    if (target && !duration)
        return 0;
    for (channel = 0; channel < 20; ++channel)
        for (field = 0; field < 6; ++field) {
            const int at = field * 80 + channel * 4;
            const uint32_t from = word(current + at);
            values[channel][field] = (float)signed_word(from) / 1000.f;
            if (target) {
                const int32_t difference = signed_word(word(target + at) - from);
                if (difference == INT32_MIN && duration == -1)
                    return 0;
                steps[channel][field] = (float)(difference / duration) / 1000.f;
            }
        }
    for (channel = 0; channel < 20; ++channel)
        for (field = 0; field < 6; ++field) {
            put_real(records[channel] + fields[field], values[channel][field]);
            if (target)
                delta[channel][field] = steps[channel][field];
        }
    return 1;
}
int lordly_transform_advance(uint8_t records[20][88], const float delta[20][6]) {
    float values[20][6];
    int channel, field;
    for (channel = 0; channel < 20; ++channel)
        for (field = 0; field < 6; ++field) {
            values[channel][field] = real(records[channel] + fields[field]) + delta[channel][field];
            if (!isfinite(values[channel][field]))
                return 0;
        }
    for (channel = 0; channel < 20; ++channel)
        for (field = 0; field < 6; ++field)
            put_real(records[channel] + fields[field], values[channel][field]);
    return 1;
}
int lordly_transform_done(int job_present) {
    return job_present == 0;
}
