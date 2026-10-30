#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04RootState {
    u8 *context;
    u8 *pointer;
    u8 value_08;
} P04RootState;

extern void func_00262B68(void);
extern void func_0024E9BC(s32, u8, s32);
extern void func_0024DC20(u8, void *, s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_802097A4[];
extern u8 D_802097B6[];
extern u8 D_802097BC[];
extern u8 D_802097F0[];
extern u8 D_802097F8[];

s32 func_0024F334(u16 message, P04RootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Preserve the real context store before reading the shared pointer. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = p01_pointer_80220F6C;
        state->value_08 = *state->pointer;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->context);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_08, D_802097A4);
        func_0024E9BC(0, state->context[0], 0x2C);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_802097F0);
        func_00262C64(0x18, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, D_802097B6);
        func_00262C64(5, 0x1003);
        break;
    case 0x1003:
        func_0024DC20(state->value_08, D_802097BC, 0);
        func_0025CAF0(state->value_08, 10, 5);
        func_0025CB60((u32)(state->pointer + 1), 15, 0);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_802097F8);
        func_00262C64(0x2C, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}

