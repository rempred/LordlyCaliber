#include "game/effect_presentation_p06_internal.h"

typedef struct P06Vec78560 { float x, y, z; } P06Vec78560;
typedef struct P06Object78560 {
    u8 unknown_00[0xA];
    u16 value_0A;
    u8 unknown_0C[4];
    s32 value_10;
    u8 unknown_14[8];
    float value_1C[4][4];
} P06Object78560;
typedef struct P06State78560 {
    u8 *value_00;
    u8 value_04, unknown_05[7];
    P06Vec78560 value_0C;
    float value_18;
    u8 unknown_1C, value_1D, value_1E, value_1F;
    u8 unknown_20[4];
    P06Object78560 *value_24[8];
} P06State78560;

extern u8 *p06_pointer_80220F68;
extern u8 p06_byte_80220F6C, p06_data_802257E6[];
extern void *p06_pointer_80220FE8;
extern const double p06_double_80225870;
s32 func_00278560(u16 message, P06State78560 *state)
{
    P06Vec78560 first, second;
    /* Two matrix arguments are committed to the observed local pair. */
    float (*pair[2])[4];
    P06Vec78560 position;
    s16 neighbor, offset, i;
    s16 flags;

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p06_pointer_80220F68;
        state->value_04 = p06_byte_80220F6C;
        neighbor = state->value_04;
        if (func_0020C014(func_0020C478(state->value_00[0]))) {
            state->value_1D = 1;
            offset = 3;
        } else {
            state->value_1D = 0;
            offset = -3;
        }
        if (state->value_04 < 20)
            neighbor = func_0024C3EC(state->value_04);
        /* The retail invalid-neighbor path loops at this location. */
        if ((u32)((s16)neighbor + (s16)offset - 20) >= 27) {
            for (;;) {}
        }
        func_0024C204(state->value_00, (struct P01Vec3 *)(&first), 0x20);
        func_0024BF04(neighbor + offset, (struct P01Vec3 *)(&second), 0x10);
        func_0024C7A4(&state->value_0C, &second, &first);
        state->value_18 = func_0025EF78(&state->value_0C) / 30.0f - p06_double_80225870;
        state->value_1E = 0;
        state->value_1F = 0;
        func_0025EFC8(&state->value_0C);
        func_00262830(0x1001);
        break;
    case 0x1001:
        flags = state->value_1F >= 7 ? 8 : 0;
        if (state->value_18 <= state->value_1F)
            flags |= 4;
        func_0024C204(state->value_00, (struct P01Vec3 *)(&position), 0x20);
        /* Only bits 2 and 3 are constructed above. Test those bits before
         * promoting the short flag value for the combined case below. */
        if ((flags & 0xC) == 0) {
            state->value_24[state->value_1E] = func_0024D180(&position, p06_data_802257E6);
            state->value_24[state->value_1E]->value_10 = 4;
        } else if (flags == 12) {
            func_002623C4(state->value_24[state->value_1E--]);
        }
        for (i = 0; i < state->value_1E; i++) {
            if (state->value_24[i]->value_10 >= 10)
                state->value_24[i]->value_10 = 6;
        }
        state->value_24[0]->value_10 = 2;
        if (flags & 4)
            func_0024CD84((struct Cd84Header *)(state->value_24[0]), 0x1000);
        else
            func_0024CD84((struct Cd84Header *)(state->value_24[0]), 0x9C00);
        if (flags & 8)
            state->value_24[state->value_1E]->value_10 = 16;
        /* Keep both real flag updates visible so their FP condition tails merge. */
        if (state->value_1D) {
            if (position.x >= 0.0f)
                state->value_24[state->value_1E]->value_0A |= 0x1000;
        } else {
            if (position.x < 0.0f)
                state->value_24[state->value_1E]->value_0A |= 0x1000;
        }
        if (!(flags & 4)) {
            u8 count;
            float distance;
            float (*matrix)[4];
            for (i = state->value_1E - 1; i >= 0; i--) {
                pair[0] = state->value_24[i]->value_1C;
                pair[1] = state->value_24[i + 1]->value_1C;
                func_0025F058(pair[0], pair[1]);
            }
            count = state->value_1F < state->value_18 ? (u32)(float)state->value_1F : (u32)state->value_18;
            distance = count * 30;
            position.x += distance * state->value_0C.x;
            position.y += distance * state->value_0C.y;
            position.z += distance * state->value_0C.z;
            matrix = state->value_24[0]->value_1C;
            func_0026C310(&state->value_0C, matrix);
            func_0025F150(matrix, &position);
        }
        if (flags & 4) {
            if (state->value_1E == 0) {
                func_002623C4(state->value_24[0]);
                func_00262C64(5, 0x101);
            } else {
                func_00262C64(1, 0x1001);
            }
        } else if (state->value_18 <= state->value_1F + 1) {
            func_00262C80();
            func_0026285C(p06_pointer_80220FE8, 0x1005);
            func_00262830(0x1001);
        } else {
            func_00262C64(1, 0x1001);
        }
        state->value_1F++;
        if (state->value_1F < 7 && state->value_1F < state->value_18)
            state->value_1E++;
        break;
    }
    return 0;
}
