#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"


/* Descriptor 11 view remains inside original data_0025e318. */
extern P05SixByteRecord D_80209994[];
extern void func_00262B68(void);

s32 func_002520C4(u16 message, void *unused)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_0024DA10(0x21, &D_80209994[0]);
        func_00262C64(0xC, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(0x21, &D_80209994[1]);
        func_00262C64(0x46, 0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
