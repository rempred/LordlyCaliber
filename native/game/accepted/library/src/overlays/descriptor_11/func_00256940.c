#include "game/effect_presentation_p06_internal.h"

typedef struct P06State56940 {
    u8 *value_00;
    u8 unknown_04[8];
    u8 value_0C;
} P06State56940;

struct P06State56410;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_data_80220F68, p06_data_80220F69;
extern u16 p06_data_80220F6A;
s32 func_00256940(u16 message, P06State56940 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        /* Scalar views retain the observed order with shared context reads. */
        (*(u8 **)state) = p06_pointer_80220F68;
        state->value_0C = p06_pointer_80220F6C[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        p06_data_80220F68 = (*(u8 **)state)[0];
        p06_data_80220F69 = state->value_0C;
        p06_data_80220F6A = 1;
        func_00262AB0(func_00256410);
        func_0024E9BC(0, (*(u8 **)state)[0], 0x91);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        p06_data_80220F68 = state->value_0C;
        p06_data_80220F69 = (*(u8 **)state)[0];
        p06_data_80220F6A = 2;
        func_00262AB0(func_00256410);
        func_0024E9BC(0, (*(u8 **)state)[0], 0x92);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        func_00262C64(0x18, 0x1003);
        break;
    case 0x1003:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
