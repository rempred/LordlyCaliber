/* Portable reconstruction of ROM 29D634/29D4F0, comparator 2A17E0 and
 * sort 20FEEC..2101DC. Original assembly: Decomp 62cc7ef4; not matching C.
 * The sort's complete 752-byte owner equals saved RAM 801CCA5C..801CCD4C
 * in the existing frame_0000 capture. A second ROM copy matches its prefix
 * but not its complete relocated branches. */
#include "scene_draw_queue.h"
#include <math.h>
#include <string.h>
static int32_t signed_word(uint32_t value) {
    return value <= 0x7fffffffU ? (int32_t)value : -1 - (int32_t)~value;
}
static uint32_t word(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static float real(const uint8_t *p) {
    uint32_t bits = word(p);
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static int integer(int32_t *out, float value) {
    if (!isfinite(value) || value < -2147483648.f || value >= 2147483648.f)
        return 0;
    *out = (int32_t)value;
    return 1;
}
static int compare(const LordlySceneDrawEntry *a, const LordlySceneDrawEntry *b, uint32_t left,
                   uint32_t right) {
    int32_t tie;
    if (a->depth != b->depth)
        return a->depth < b->depth ? -1 : 1;
    if (a->y != b->y)
        return a->y < b->y ? 1 : -1;
    tie = signed_word((left - right) * 0xcccccccdU);
    // Explicit arithmetic right shift; avoid implementation-defined signed shifts.
    return tie >= 0 ? tie / 4 : -1 - (int32_t)((uint32_t)(-1 - tie) / 4);
}
static int sort(LordlySceneDrawEntry *rows, int count, LordlySceneDrawLayout layout) {
    int low = 0, high = count - 1, pending = 0, lows[12], highs[12];
    int left, right, pivot, i;
    LordlySceneDrawEntry temporary;
    if (count < 2)
        return 1;
    for (;;) {
        if (high - low < 11) {
            if (!pending)
                break;
            --pending;
            low = lows[pending];
            high = highs[pending];
        }
        left = low;
        right = high;
        pivot = (low + high) / 2;
        for (;;) {
            while (compare(rows + left, rows + pivot, layout.queue + left * 20U,
                           layout.queue + pivot * 20U) < 0)
                ++left;
            while (compare(rows + pivot, rows + right, layout.queue + pivot * 20U,
                           layout.queue + right * 20U) < 0)
                --right;
            if (left >= right)
                break;
            temporary = rows[left];
            rows[left] = rows[right];
            rows[right] = temporary;
            ++left;
            --right;
        }
        if (pending >= 12)
            return 0;
        if (high - right < left - low) {
            if (left - low >= 11) {
                lows[pending] = low;
                highs[pending++] = left - 1;
            }
            low = right + 1;
        } else {
            if (high - right >= 11) {
                lows[pending] = right + 1;
                highs[pending++] = high;
            }
            high = left - 1;
        }
    }
    for (i = 1; i < count; ++i) {
        temporary = rows[i];
        left = i - 1;
        while (left >= 0 &&
               compare(&temporary, rows + left, layout.temporary, layout.queue + left * 20U) < 0) {
            rows[left + 1] = rows[left];
            --left;
        }
        rows[left + 1] = temporary;
    }
    return 1;
}
int lordly_scene_draw_queue(LordlySceneDrawEntry out[284], int *out_count,
                            const uint8_t *const actors[28], const uint8_t *const objects[256],
                            int object_count, const float camera[14],
                            LordlySceneDrawLayout layout) {
    LordlySceneDrawEntry rows[284];
    float dx, dz, magnitude, reciprocal, bx, bz, translation;
    int count = 0, i;
    if (object_count < 0 || object_count > 256)
        return 0;
    dx = camera[8] - camera[5];
    dz = camera[10] - camera[7];
    magnitude = sqrtf((dx * dx + 0.f) + dz * dz);
    reciprocal = (float)(-1. / (double)magnitude);
    bx = dx * reciprocal;
    bz = dz * reciprocal;
    translation = -((camera[5] * bx + 0.f) + camera[7] * bz);
    if (!isfinite(bx) || !isfinite(bz) || !isfinite(translation))
        return 0;
    for (i = 0; i < object_count; ++i) {
        const uint8_t *p = objects[i];
        if (!p || p[0x14] < 1 || p[0x14] > 3)
            continue;
        rows[count].x = signed_word(word(p + 4));
        rows[count].y = signed_word(word(p + 8));
        rows[count].z = signed_word(word(p + 12));
        rows[count].kind = p[0x14] == 1 ? 0 : p[0x14];
        rows[count++].index = (int16_t)i;
    }
    for (i = 0; i < 28; ++i) {
        const uint8_t *p = actors[i];
        if (!p)
            continue;
        if (!integer(&rows[count].x, real(p + 0x11c) * 30.f) ||
            !integer(&rows[count].y, real(p + 0x120) * 30.f) ||
            !integer(&rows[count].z, real(p + 0x124) * 30.f))
            return 0;
        rows[count].kind = 1;
        rows[count++].index = (int16_t)i;
    }
    for (i = 0; i < count; ++i) {
        const float depth =
            ((bx * (float)rows[i].x + 0.f * (float)rows[i].y) + bz * (float)rows[i].z) +
            translation;
        if (!integer(&rows[i].depth, depth))
            return 0;
    }
    if (!sort(rows, count, layout))
        return 0;
    memcpy(out, rows, count * sizeof(*rows));
    *out_count = count;
    return 1;
}
/* Selection and ordering from ROM2AA62C..2AAB88. Display-list matrices,
 * materials and callback drawing are separate owners. */
int lordly_scene_mode_two_passes(LordlySceneWorldDrawPass out[852], int *out_count,
                                 const LordlySceneDrawEntry *queue, int queue_count,
                                 const uint8_t *const actors[28], const uint8_t *const objects[256],
                                 const int16_t shadow_alpha[2]) {
    LordlySceneWorldDrawPass passes[852];
    int i, j, count = 0;
    if (queue_count < 0 || queue_count > 284)
        return 0;
    for (i = 0; i < queue_count; ++i) {
        const int index = queue[i].index, kind = queue[i].kind;
        if (kind == 1) {
            if (index < 0 || index >= 28 || !actors[index])
                return 0;
            if (actors[index][0x141] < 2)
                for (j = 0; j < 2; ++j)
                    if (shadow_alpha[j] != 0) {
                        passes[count].kind = 1;
                        passes[count].index = (int16_t)index;
                        passes[count].shadow = (int16_t)j;
                        passes[count++].alpha = (uint8_t)shadow_alpha[j];
                    }
        } else if (kind == 0 || kind == 2 || kind == 3) {
            if (index < 0 || index >= 256 || !objects[index])
                return 0;
        }
    }
    for (i = 0; i < queue_count; ++i) {
        const int index = queue[i].index, kind = queue[i].kind;
        if (kind == 1) {
            if (actors[index][0x141] & 1)
                continue;
        } else if (kind != 0 && kind != 2 && kind != 3)
            continue;
        passes[count].kind = (int16_t)kind;
        passes[count].index = (int16_t)index;
        passes[count].shadow = -1;
        passes[count++].alpha = 255;
    }
    memcpy(out, passes, count * sizeof(*passes));
    *out_count = count;
    return 1;
}
/* Selection and ordering from ROM 2AFC68..2B04FC; display-list construction and
 * drawing callbacks belong to their separate owners. */
int lordly_scene_mode_zero_passes(LordlySceneDrawPass out[568], int *out_count,
                                  const LordlySceneDrawEntry *queue, int queue_count,
                                  const uint8_t *const actors[28], uint8_t object_channel,
                                  uint8_t channel) {
    LordlySceneDrawPass passes[568];
    int i, count = 0;
    if (queue_count < 0 || queue_count > 284)
        return 0;
    for (i = 0; i < queue_count; ++i) {
        const int index = queue[i].index;
        if (queue[i].kind == 0) {
            if (index < 0 || index >= 256)
                return 0;
        } else if (index < 0 || index >= 28 || !actors[index]) {
            return 0;
        }
        if (queue[i].kind == 1 && actors[index][0x141] < 2 && actors[index][0x13e] == channel) {
            passes[count].kind = 2;
            passes[count++].index = (int16_t)index;
        }
    }
    for (i = 0; i < queue_count; ++i) {
        const int index = queue[i].index;
        if (queue[i].kind == 0) {
            if (object_channel != channel)
                continue;
            passes[count].kind = 0;
        } else {
            if ((actors[index][0x141] & 1) || actors[index][0x13e] != channel)
                continue;
            passes[count].kind = 1;
        }
        passes[count++].index = (int16_t)index;
    }
    memcpy(out, passes, count * sizeof(*passes));
    *out_count = count;
    return 1;
}
