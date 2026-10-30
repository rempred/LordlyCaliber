#include "common/types.h"
#include "game/effect_presentation_p04.h"

extern void func_00262B68(void);
extern u8 *p01_pointer_80220F6C;
extern u8 D_8020A1E8[];

s32 func_0025DE78(u16 message, u8 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *state = *p01_pointer_80220F6C;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(*state, D_8020A1E8);
        func_00262C64(0x12, 0x1001);
        break;
    case 0x1001:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
