#include "game/effect_presentation_p06_internal.h"

typedef struct P06State76998 { u8 *value_00, *value_04; } P06State76998;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
s32 func_00276998(u16 message, P06State76998 *state)
{
    s32 result = 0;

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
        func_00262830(0x1003);
        /* This message also executes the following handoff. */
    case 0x1003:
        p06_pointer_80220F6C = state->value_04;
        func_00262AB0(func_002767bc);
        func_00262C64(-1, 0x1004);
        break;
    case 0x1004:
        func_0025CAF0(state->value_04[0], 0, 40);
        func_0025CB60((u32)(state->value_04 + 1), 35, 15);
        func_00262C64(80, 0x1005);
        break;
    case 0x1005:
        func_002628E4(0x101);
        break;
    default:
        result = 0;
        break;
    }
    return result;
}
