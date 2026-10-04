#include "ui_resource.h"
static unsigned half(const uint8_t *p) {
    return (unsigned)p[0] * 256 + p[1];
}
static int signed_half(unsigned v) {
    v &= 65535U;
    return v < 32768U ? (int)v : (int)v - 65536;
}
static void put_half(uint8_t *p, unsigned v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
static int shift8(int value) {
    return value >= 0 ? value / 256 : -((-value + 255) / 256);
}
static int opening_axis(uint8_t *record, int axis, int mode, int *fraction) {
    const int low = 6 + axis * 2, high = 10 + axis * 2, extent = 0x28 + axis * 2;
    const int size = signed_half(half(record + extent));
    const int step = size * 256 / record[4];
    const int old_fraction = axis ? (int)(half(record + 0x2c) >> 8) : record[0x2d];
    const int edge = axis ? 239 : 319;
    const int grow = mode == 0 || mode == (axis ? 2 : 1);
    int done = 0, current, bound;
    if (mode == 3)
        return 0;
    if (grow) {
        *fraction = signed_half(half(record + high)) * 256 + old_fraction + step;
        put_half(record + high, (unsigned)shift8(*fraction));
        current = signed_half(half(record + high));
        bound = signed_half(half(record + low)) + size - 1;
        if (current >= bound) {
            put_half(record + high, (unsigned)bound);
            done = 1;
        }
        current = signed_half(half(record + high));
        if (current > edge) {
            put_half(record + low, half(record + low) + (unsigned)(edge - current));
            put_half(record + high, (unsigned)edge);
            if (signed_half(half(record + low)) < 0) {
                put_half(record + low, 0);
                done = 1;
            }
        }
    } else {
        *fraction = signed_half(half(record + low)) * 256 + old_fraction - step;
        put_half(record + low, (unsigned)shift8(*fraction));
        current = signed_half(half(record + low));
        bound = signed_half(half(record + high)) - size + 1;
        if (current <= bound) {
            put_half(record + low, (unsigned)bound);
            done = 1;
        }
        current = signed_half(half(record + low));
        if (current < 0) {
            put_half(record + high, half(record + high) - (unsigned)current);
            put_half(record + low, 0);
            if (signed_half(half(record + high)) > edge) {
                put_half(record + high, (unsigned)edge);
                done = 1;
            }
        }
    }
    return done;
}
int lordly_ui_opening(LordlyUiPool *pool, const LordlyUiQueue *queue) {
    int i, horizontal = 0, vertical = 0;
    if (queue->count > LORDLY_UI_SLOTS)
        return -1;
    for (i = 0; i < queue->count; ++i)
        if (queue->entries[i] >= LORDLY_UI_SLOTS)
            return -1;
    for (i = 0; i < queue->count; ++i) {
        uint8_t *record = pool->records[queue->entries[i]].bytes;
        unsigned flags = half(record);
        int done;
        if ((flags & 0xe800) != 0xe800 || (record[3] & 2))
            continue;
        if (!record[4])
            return -2;
        if (!(flags & 0x400)) {
            put_half(record + 0x2c, 0);
            put_half(record, flags | 0x400);
        }
        done = opening_axis(record, 0, record[2] >> 6, &horizontal);
        done |= opening_axis(record, 1, (record[2] >> 4) & 3, &vertical) << 1;
        /* Original storage swaps byte lanes; skipped axes retain the previous
         * record's accumulator within this one queue pass. */
        put_half(record + 0x2c, ((unsigned)horizontal & 255U) * 256 + ((unsigned)vertical & 255U));
        if (done == 3)
            put_half(record, half(record) & 0xf3ff);
    }
    return 0;
}
static int32_t signed_word(uint32_t value) {
    return value < 0x80000000U ? (int32_t)value : -1 - (int32_t)(0xffffffffU - value);
}
static int32_t close_edge(const uint8_t *record, int axis, int32_t margin) {
    return signed_word((uint32_t)signed_half(half(record + 6 + axis * 2)) + (uint32_t)margin - 1U);
}
int lordly_ui_closing(LordlyUiRecord *item, int32_t margin) {
    uint8_t *record = item->bytes;
    const unsigned flags = half(record);
    int horizontal, vertical, right, bottom;
    unsigned remainder;
    if ((flags & 0xf000) != 0xf000 || (record[3] & 2))
        return 0;
    if (signed_half(half(record + 10)) == close_edge(record, 0, margin) &&
        signed_half(half(record + 12)) == close_edge(record, 1, margin))
        return 1;
    if (!(flags & 0x400)) {
        if (!record[4])
            return -2;
        horizontal =
            (signed_half(half(record + 10)) - signed_half(half(record + 6)) + 1) * 256 / record[4];
        vertical =
            (signed_half(half(record + 12)) - signed_half(half(record + 8)) + 1) * 256 / record[4];
        put_half(record + 0x2e, 0);
        put_half(record, flags | 0x400);
        put_half(record + 0x28, (unsigned)shift8(horizontal));
        put_half(record + 0x2a, (unsigned)shift8(vertical));
        put_half(record + 0x2c, ((unsigned)vertical & 255U) * 256 + ((unsigned)horizontal & 255U));
    }
    horizontal = signed_half(half(record + 0x28)) * 256 + record[0x2d];
    vertical = signed_half(half(record + 0x2a)) * 256 + record[0x2c];
    right = signed_half(half(record + 10)) * 256 + record[0x2f] - horizontal;
    bottom = signed_half(half(record + 12)) * 256 + record[0x2e] - vertical;
    remainder = ((unsigned)right & 255U) + ((unsigned)bottom & 255U) * 256;
    put_half(record + 10, (unsigned)shift8(right));
    put_half(record + 12, (unsigned)shift8(bottom));
    put_half(record + 0x2e, remainder);
    if (signed_half(half(record + 10)) < close_edge(record, 0, margin))
        put_half(record + 10, (unsigned)close_edge(record, 0, margin));
    if (signed_half(half(record + 12)) < close_edge(record, 1, margin))
        put_half(record + 12, (unsigned)close_edge(record, 1, margin));
    return 0;
}
