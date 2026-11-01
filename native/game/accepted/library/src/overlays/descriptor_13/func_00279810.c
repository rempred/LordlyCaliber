#include "game/effect_presentation_p06_internal.h"

#include "game/effect_presentation_p04.h"

extern u8 *p06_pointer_80220F68, *p06_pointer_80220F6C;
extern u8 p06_byte_80220F68, p06_byte_80220F69;
s32 func_00279810(u16 message, u8 *state)
{
    s32 result = 0;
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state[0] = p06_pointer_80220F68[0];
        state[1] = p06_pointer_80220F6C[0];
        func_00262830(0x1000);
        break;
    case 0x1000:
        p06_byte_80220F68 = state[0];
        p06_byte_80220F69 = state[1];
        func_00262AB0(func_00279460);
        func_00262C64(-1, 0x1001);
        break;
    case 0x1001:
        func_00262C64(20, 0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    default:
        result = 0;
        break;
    }
    return result;
}
