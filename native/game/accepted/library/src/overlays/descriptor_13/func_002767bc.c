#include "game/effect_presentation_p06_internal.h"

typedef struct P06State767BC { u8 *value_00, *value_04; } P06State767BC;
extern u8 *p06_pointer_80220F6C;
extern u8 p06_data_802256CA[], p06_data_802256C4[], p06_data_802256D0[];
extern u8 p06_data_802256E8[], p06_data_802256D6[], p06_data_802256DC[];
s32 func_002767bc(u16 message, P06State767BC *state)
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
        u8 *records = p06_data_802256CA;
        func_0024E9BC(0, state->value_00[0], 0x21);
        func_0024DA10(state->value_04[0], records);
        func_0024DA10(state->value_04[0], records + 0x18);
        func_00262C64(5, 0x1002);
        break;
    }
    case 0x1002:
        func_0024DA10(state->value_04[0], p06_data_802256C4);
        func_00262C64(7, 0x1003);
        break;
    case 0x1003:
        func_00262C80();
        func_00262C64(12, 0x1004);
        break;
    case 0x1004:
        func_0024DA10(state->value_04[0], p06_data_802256D0);
        func_00262C64(3, 0x1005);
        break;
    case 0x1005:
        func_0024DA10(state->value_04[0], p06_data_802256E8);
        func_00262C64(13, 0x1006);
        break;
    case 0x1006:
        func_0024DA10(state->value_04[0], p06_data_802256D6);
        func_00262C64(9, 0x1007);
        break;
    case 0x1007:
        func_0024DA10(state->value_04[0], p06_data_802256DC);
        func_00262C64(35, 0x101);
        break;
    }
    return 0;
}
