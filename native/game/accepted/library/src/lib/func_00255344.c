#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04RootState {
    u8 *context;
    u8 *pointer;
    u8 value_08;
} P04RootState;

extern void func_00262B68(void);

extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_80209C7A[];
extern u8 D_80209D14[];
extern u8 D_80209D1C[];

s32 func_00255344(u16 message, P04RootState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
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
        func_0024DA10(state->value_08, D_80209C7A);
        func_0024DA10(state->value_08, D_80209C7A + 24);
        func_0024E9BC(0, state->context[0], 0x85);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209D14);
        func_00262C64(0xD, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, D_80209C7A + 18);
        func_00262830(0x1003);
        break;
    case 0x1003:
        func_0024DA10(state->value_08, D_80209C7A + 6);
        func_00262C64(0x1C, 0x1004);
        break;
    case 0x1004:
        func_0024DA10(state->value_08, D_80209C7A + 12);
        func_0025CAF0(state->value_08, 3, 5);
        func_0025CB60((u32)(state->pointer + 1), 5, 0);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_80209D1C);
        func_00262C64(0x23, 0x1005);
        break;
    case 0x1005:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
