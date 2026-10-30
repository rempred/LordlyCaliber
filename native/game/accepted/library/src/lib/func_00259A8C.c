#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Value0CState {
    u8 *context;
    u8 *pointer;
    u8 unknown_08[4];
    u8 value_0C;
} P04Value0CState;

extern void func_00262B68(void);
extern void func_0024E9BC(s32, u8, s32);
extern u8 *p01_context_80220F68;
extern u8 *p01_pointer_80220F6C;
extern u8 D_80209F62[];
extern u8 D_80209F68[];
extern u8 D_80209F6E[];
extern u8 D_80209F74[];

s32 func_00259A8C(u16 message, P04Value0CState *state)
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
        func_0024DA10(state->value_0C, D_80209F62);
        func_0024E9BC(0, state->context[0], 0xA5);
        func_00262C64(2, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_0C, D_80209F68);
        func_00262C64(9, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(state->value_0C, D_80209F6E);
        func_0024DA10(state->value_0C, D_80209F6E + 12);
        func_0025CAF0(state->value_0C, 0x19, 0xA);
        func_0025CB60((u32)(state->pointer + 1), 0x1B, 5);
        func_00262C64(0x15, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(state->value_0C, D_80209F74);
        func_00262C64(0x19, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
