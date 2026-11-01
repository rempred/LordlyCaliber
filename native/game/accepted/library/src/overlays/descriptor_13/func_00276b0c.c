#include "game/effect_presentation_p06_internal.h"

typedef struct P06State76B0C { u8 *value_00, *value_04; } P06State76B0C;
extern u8 *p06_pointer_80220F6C;
extern u8 p06_data_802256F4[], p06_data_802256EE[], p06_data_802256FA[];
extern u8 p06_data_80225712[], p06_data_80225700[], p06_data_80225706[];
s32 func_00276b0c(u16 message, P06State76B0C *state)
{
    switch (message) {
    case 0x101:
        func_00262C80();
        func_00262B68();
        break;
    case 0x100:
        state->value_04 = p06_pointer_80220F6C;
        func_00262830(0x1001);
        break;
    case 0x1001: {
        u8 *records = p06_data_802256F4;
        func_0024E9BC(0, state->value_00[0], 0x22);
        func_0024DA10(state->value_04[0], records);
        func_0024DA10(state->value_04[0], records + 0x18);
        func_00262C64(5, 0x1002);
        break;
    }
    case 0x1002:
        func_0024DA10(state->value_04[0], p06_data_802256EE);
        func_00262C64(1, 0x1003);
        break;
    case 0x1003:
        func_00262C80();
        func_00262C64(1, 0x1004);
        break;
    case 0x1004:
        func_0024DA10(state->value_04[0], p06_data_802256FA);
        func_00262C64(4, 0x1005);
        break;
    case 0x1005:
        func_0024DA10(state->value_04[0], p06_data_80225712);
        func_00262C64(30, 0x1006);
        break;
    case 0x1006:
        func_0024DA10(state->value_04[0], p06_data_80225700);
        func_00262C64(9, 0x1007);
        break;
    case 0x1007:
        func_0024DA10(state->value_04[0], p06_data_80225706);
        func_00262C64(35, 0x101);
        break;
    }
    return 0;
}
