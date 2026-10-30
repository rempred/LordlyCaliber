#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04ListState {
    u8 *context;
    u8 *volatile pointer;
    u8 value_08;
    u8 unknown_09;
    s16 count;
} P04ListState;

extern void func_00262B68(void);
extern void func_0024E9BC(s32, u8, s32);
extern u8 *p01_context_80220F68;
/* This access view retains the two observed shared-pointer loads. */
extern u8 *volatile D_80220F6C;
extern u8 D_800EB240[];
extern u8 D_802097C2[];
extern u8 D_80209800[];
extern u8 D_80209808[];

s32 func_0024F4F4(u16 message, P04ListState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        u8 first;
        u8 *base;

        /* Preserve the context store before the following shared load. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = D_80220F6C;
        first = *D_80220F6C;
        base = state->pointer;
        /* Capture the byte before potentially aliasing state writes. */
        state->count = 0;
        state->value_08 = first;
        if (*base != 0xFF) {
            s32 end = 0xFF;
            u8 *cursor = base;
            do {
                cursor++;
                state->count++;
            } while (*cursor != end);
        }
        func_00262830(0x1000);
        break;
    }
    case 0x1000: {
        s16 next = state->count == 1 ? 0x1001 : 0x1002;

        state->count = 0;
        func_0025C8A4(state->context);
        func_00262C64(-1, next);
        break;
    }
    case 0x1001:
        func_0024DA10(state->value_08, D_802097C2);
        func_0024E9BC((s32)D_800EB240, state->context[0], 0x2D);
        func_0025CAF0(state->value_08, 8, 5);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209800);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209808);
        func_00262C64(0x1C, 0x1003);
        break;
    case 0x1002: {
        u8 value = state->pointer[state->count];

        if (value != 0xFF) {
            func_0024DA10(value, D_802097C2);
            func_0024E8DC(state->context[0], 0x2D);
            func_0025CAF0(value, 8, 5);
        }
        if (state->pointer[++state->count] == 0xFF) {
            func_00262C64(0x1C, 0x1003);
        } else {
            func_00262C64(5, 0x1002);
        }
        break;
    }
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}


