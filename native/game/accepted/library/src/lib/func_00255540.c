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
extern void func_0024E9BC(s32, u8, s32);
extern void func_0024C57C(u8, P04Vec3 *, s32);
extern void *func_0024D180(P04Vec3 *, void *);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_80209C98[];
extern u8 D_80209CB6[];
extern u8 D_80209D24[];
extern s32 D_80209D2C[];
extern u8 D_80209D3C[];

s32 func_00255540(u16 message, P04CounterState *state)
{
    P04Vec3 positions[4];

    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = p01_pointer_80220F6C;
        state->value_08 = *state->pointer;
        state->counter_0A = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_08, D_80209C98);
        func_0024DA10(state->value_08, D_80209C98 + 24);
        func_0024E9BC(0, state->context[0], 0x86);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209D24);
        func_00262C64(0xD, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, D_80209C98 + 18);
        func_00262830(0x1003);
        break;
    case 0x1003:
        func_0024DA10(state->value_08, D_80209C98 + 6);
        func_00262C64(0x1C, 0x1004);
        break;
    case 0x1004:
        func_0024DA10(state->value_08, D_80209C98 + 12);
        func_0025CAF0(state->value_08, 8, 5);
        func_0025CB60((u32)(state->pointer + 1), 0xC, 0);
        func_00262C64(4, 0x1005);
        break;
    case 0x1005:
        func_0024C57C(state->value_08, positions, 0);
        func_0024D180(&positions[D_80209D2C[state->counter_0A]], D_80209CB6);
        if (state->counter_0A++ == 3) {
            func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209D3C);
            func_00262C64(0x17, 0x1006);
        } else {
            func_00262C64(2, 0x1005);
        }
        break;
    case 0x1006:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
