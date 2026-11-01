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
extern u8 D_802097A4[];
extern u8 D_802097AA[];
extern u8 D_802097B0[];
extern u8 D_802097E0[];
extern u8 D_802097E8[];

s32 func_0024F174(u16 message, P04RootState *state)
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
        func_0024E9BC(0, state->context[0], 0x2B);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_802097E0);
        func_00262C64(0x18, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_08, D_802097AA);
        func_00262C64(5, 0x1003);
        break;
    case 0x1003:
        func_0024DC20(state->value_08, (struct Dc20InputRecord *)(D_802097B0), 0);
        func_0025CAF0(state->value_08, 5, 5);
        func_0025CB60((u32)(state->pointer + 1), 10, 0);
        func_0025DAB0(state->value_08, (const struct Func0025DAB0Input *)D_802097E8);
        func_00262C64(0x1D, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
