#include "game/effect_presentation_p06_internal.h"

typedef struct P06State778D4 { u8 *value_00, *value_04; } P06State778D4;
extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
s32 func_002778D4(u16 message, P06State778D4 *state)
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
        p06_pointer_80220F6C = *(u8 **)((u8 *)state + 4);
        /* Scalar views preserve the context store before the source reload. */
        func_0024E9BC(0, (*(u8 **)state)[0], 0x3F);
        func_00262AB0(func_00277CCC);
        func_00262AB0(func_00277D60);
        func_00262830(0x1003);
        break;
    case 0x1003:
        func_0025CAF0(state->value_04[0], 25, 40);
        func_0025CB60((u32)(state->value_04 + 1), 30, 20);
        func_00262C64(70, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
