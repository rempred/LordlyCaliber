#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Value0CState {
    u8 *context;
    u8 *pointer;
    u8 unknown_08[4];
    u8 value_0C;
} P04Value0CState;

extern void func_00262B68(void);


extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_8020A05E[];
extern u8 D_8020A070[];

s32 func_0025A6B0(u16 message, P04Value0CState *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Retain the observed context store before the shared-pointer load. */
        *(u8 **)state = p01_context_80220F68;
        state->pointer = p01_pointer_80220F6C;
        state->value_0C = *state->pointer;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024E490(state->value_0C, D_8020A05E, 3);
        func_0024DA10(state->value_0C, D_8020A05E + 0x18);
        func_0024E9BC(0, state->context[0], 0xAE);
        func_0025CAF0(state->value_0C, 0xA, 0xA);
        func_0025CB60((u32)(state->pointer + 1), 0x23, 5);
        func_00262C64(0x13, 0x1001);
        break;
    case 0x1001:
        func_0024DA50(state->value_0C, D_8020A070);
        func_00262C64(0x2D, 0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
