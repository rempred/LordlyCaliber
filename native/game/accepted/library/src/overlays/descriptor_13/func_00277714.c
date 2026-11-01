#include "game/effect_presentation_p06_internal.h"

typedef struct P06State77714 { u8 *value_00, *value_04; } P06State77714;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_data_80225748[];
s32 func_00277714(u16 message, P06State77714 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *(u8 **)state = p06_pointer_80220F68;
        state->value_04 = p06_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0025C8A4(state->value_00);
        func_00262C64(-1, 0x1002);
        break;
    case 0x1002:
        /* Scalar views retain both context stores and the following reload. */
        p06_pointer_80220F68 = *(u8 **)state;
        p06_pointer_80220F6C = *(u8 **)((u8 *)state + 4);
        func_0024E9BC(0, (*(u8 **)state)[0], 0x3D);
        if (state->value_00[0] != state->value_04[0]) {
            func_00262AB0(func_00276fe0);
            func_00262C64(-1, 0x1003);
        } else {
            func_00262830(0x101);
        }
        break;
    case 0x1003:
        func_0024DA50(state->value_04[0], p06_data_80225748);
        func_0024E9BC(0, state->value_00[0], 0x3E);
        func_0025CAF0(state->value_04[0], 5, 45);
        func_0025CB60((u32)(state->value_04 + 1), 20, 30);
        func_00262C64(60, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
