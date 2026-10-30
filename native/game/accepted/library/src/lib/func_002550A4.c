#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Vec3 {
    float x;
    float y;
    float z;
} P04Vec3;

typedef struct P04CounterState {
    u8 *context;
    u8 *pointer;
    u8 value_08;
    u8 unknown_09;
    s16 counter_0A;
} P04CounterState;

extern void func_00262B68(void);
extern void func_0024DC20(u8, void *, s32);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0024C57C(u8, P04Vec3 *, s32);
extern s16 func_0024C2F8(P04Vec3 *);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_80209C68[];
extern u8 D_80209C6E[];
extern u8 D_80209CE4[];
extern u8 D_80209CEC[];
extern u8 D_80209CF4[];
extern s16 D_80209CFC[];
extern s16 D_80209D04[];
extern u8 D_80209D0C[];

s32 func_002550A4(u16 message, P04CounterState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100: {
        u8 first;

        /* Capture the byte before the following state writes. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = p01_pointer_80220F6C;
        first = *state->pointer;
        state->counter_0A = 0;
        state->value_08 = first;
        func_00262830(0x1000);
        break;
    }
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_08, D_80209C68);
        func_0024DC20(state->value_08, D_80209C68 + 12, 6);
        func_0024E9BC(0, state->context[0], 0x84);
        func_0025CAF0(state->value_08, 7, 0xA);
        if (state->pointer[1] == 0xFF) {
            func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209CE4);
            func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209CEC);
            func_00262C64(0x2D, 0x1003);
        } else {
            func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209CF4);
            func_00262C64(0xD, 0x1002);
        }
        break;
    case 0x1002: {
        P04Vec3 positions[4];
        s16 value;

        func_0024C57C(state->value_08, positions, 0);
        value = func_0024C2F8(&positions[D_80209D04[state->counter_0A]]);
        if (value != -1) {
            s16 index;

            for (index = 0; state->pointer[index] != (u8)value; index++) {
            }
            func_0024DA10(state->pointer[index], D_80209C6E);
            func_0025CAF0(state->pointer[index], 7, 0);
        }
        if (state->counter_0A == 3) {
            func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209D0C);
            func_00262C64(0x12, 0x1003);
        } else {
            func_00262C64(D_80209CFC[state->counter_0A++], 0x1002);
        }
        break;
    }
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
