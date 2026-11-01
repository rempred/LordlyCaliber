#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

extern u8 p01_byte_80220F68;
/* Two six-byte records retained in original data_0025e890. */
extern u8 D_80209F32[];
extern void func_00262B68(void);

s32 func_00259FD4(u16 message, u8 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        state[0] = p01_byte_80220F68;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_00262C64(0xF, 0x1001);
        break;
    case 0x1001:
        func_0024E490(state[0], D_80209F32, 2);
        func_00262C64(0x1D, 0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
