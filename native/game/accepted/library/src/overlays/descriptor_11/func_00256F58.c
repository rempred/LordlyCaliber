#include "game/effect_presentation_p06_internal.h"

typedef struct P06State56F58 {
    u8 *value_00;
    u8 *value_04;
    u32 unknown_08;
    u8 value_0C, unknown_0D;
    s16 value_0E;
} P06State56F58;

struct P06State56AC8;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_data_80220F68, p06_data_80220F69;
extern u16 p06_data_80220F6A;
extern u8 p06_data_80209E1E[], p06_data_80209E3C[];
extern s16 p06_data_80209FD0[];
s32 func_00256F58(u16 message, P06State56F58 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Scalar view preserves the first-field store before the global load. */
        *(u8 **)state = p06_pointer_80220F68;
        state->value_04 = p06_pointer_80220F6C;
        state->value_0C = state->value_04[0];
        state->value_0E = 0;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(state->value_00[0], p06_data_80209E1E);
        func_0024E9BC(0, state->value_00[0], 0x93);
        func_00262C64(21, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(state->value_00[0], p06_data_80209E3C);
        func_00262C64(6, 0x1002);
        break;
    case 0x1002:
        p06_data_80220F68 = state->value_00[0];
        p06_data_80220F69 = state->value_0C;
        /* Keep the shared byte store before this observed halfword read. */
        p06_data_80220F6A = *(u16 *)((u8 *)state + 0xE);
        func_00262AB0(func_00256AC8);
        if (state->value_0E == 2) {
            func_00262C64(-1, 0x1003);
        } else {
            func_00262C64(p06_data_80209FD0[state->value_0E], 0x1002);
        }
        state->value_0E++;
        break;
    case 0x1003:
        func_00262C64(20, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
