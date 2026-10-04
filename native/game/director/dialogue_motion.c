/* Reconstructed from Rev 0 ROM EA184..EA628, loaded at RAM 8019D404.
 * Keep signed comparisons after halfword stores and the retained completion flag. */
#include "dialogue_motion.h"
static unsigned half(const uint8_t *p) {
    return (unsigned)p[0] * 256 + p[1];
}
static int signed_half(unsigned value) {
    value &= 65535U;
    return value < 32768U ? (int)value : (int)value - 65536;
}
static void put_half(uint8_t *p, int value) {
    p[0] = (uint8_t)((unsigned)value >> 8);
    p[1] = (uint8_t)value;
}
void lordly_dialogue_move(uint8_t state[1144]) {
    int axis;
    for (axis = 0; axis < 2; ++axis) {
        const int current = signed_half(half(state + 0x24 + axis * 2));
        const int target = signed_half(half(state + 0x28 + axis * 2));
        const int step = (int)half(state + 0x38);
        int next;
        if (target < current) {
            next = signed_half((unsigned)(current - step));
            if (next < target)
                next = target;
        } else {
            next = signed_half((unsigned)(current + step));
            if (next > target)
                next = target;
        }
        put_half(state + 0x24 + axis * 2, next);
    }
    if (half(state + 0x24) == half(state + 0x28) && half(state + 0x26) == half(state + 0x2a))
        state[0x3e] = 1;
}
void lordly_dialogue_bounds(uint8_t window[168], uint8_t state[1144]) {
    const int margin = state[0x40] < 128 ? state[0x40] : (int)state[0x40] - 256;
    int axis;
    for (axis = 0; axis < 2; ++axis) {
        const int size =
            (int)half(window + 10 + axis * 2) - (int)half(window + 6 + axis * 2) + 1 + margin * 2;
        const int start =
            (int)half(state + 0x24 + axis * 2) - (int)(((unsigned)size & 65535U) >> 1);
        put_half(window + 6 + axis * 2, start);
        put_half(window + 10 + axis * 2, start + size - 1);
    }
    state[0x56] = 255;
}
void lordly_dialogue_pointer(const uint8_t window[168], uint8_t state[1144]) {
    const int bare = window[0x91] & 4;
    const int portrait =
        state[0x47] == 0 && state[0x3d] == 0 && signed_half(half(state + 0x14)) >= 0;
    const int left = bare ? 0 : state[0x47] >= 2 ? 7 : portrait ? 56 : 8;
    const int right = (int)half(state + 0x1a) * 7 + (bare               ? -9
                                                     : state[0x47] >= 2 ? -2
                                                     : portrait         ? 47
                                                                        : -1);
    const int origin = (int)half(state + 0x28) - signed_half(half(state + 0x20)) / 2 +
                       (state[0x3a] == 0 ? 20 : -12);
    int pointer = signed_half((unsigned)((int)half(window + 0x8c) - origin));
    if (pointer < left)
        pointer = left;
    if (pointer > right)
        pointer = right;
    put_half(state + 0x30, pointer);
}
