#include "common/types.h"

extern u8 p01_byte_80220F68;
extern u8 p03_data_80209CC8[];
extern u8 p03_data_80209CC2[];
extern void func_00262B68(void);
extern void func_00262830(s32);
extern void func_002628E4(s32);
extern void func_00262C64(s16, s16);
extern void *func_0024DA10(u32, void *);

s32 func_0025630C(u16 message, u8 *state)
{
    switch (message) {
    case 0x101:
        func_00262B68();
        break;
    case 0x100:
        *state = p01_byte_80220F68;
        func_00262830(0x1001);
        break;
    case 0x1001:
        func_00262C64(0x15, 0x1002);
        break;
    case 0x1002:
        func_0024DA10(*state, p03_data_80209CC8);
        func_00262C64(0x13, 0x1003);
        break;
    case 0x1003:
        func_0024DA10(*state, p03_data_80209CC2);
        func_00262C64(0x28, 0x1004);
        break;
    case 0x1004:
        func_002628E4(0x101);
        break;
    }
    return 0;
}
