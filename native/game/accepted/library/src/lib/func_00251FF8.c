#include "common/types.h"

extern u8 p01_byte_80220F68;
extern u8 p03_data_802098A6[];
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);
extern void *func_0024DA10(u32, void *);

s32 func_00251FF8(u16 message, u8 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *state = p01_byte_80220F68;
        func_00262830(0x1000);
        break;
    case 0x1000:
        func_00262C64(0x1A, 0x1001);
        break;
    case 0x1001:
        func_0024DA10(*state, p03_data_802098A6);
        func_00262830(0x1002);
        break;
    case 0x1002:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
