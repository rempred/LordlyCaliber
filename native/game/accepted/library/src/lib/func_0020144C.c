#include "common/types.h"

s32 func_0020144C(u8 code)
{
    if ((u8)(code - 0x11) < 0x1C) {
        return (u8)(code + 0x50);
    }
    if ((u8)(code + 0x6E) < 8) {
        return (u8)(code + 0xEB);
    }
    if ((u8)(code - 0x37) < 0x24) {
        return (u8)(code + 0xC9);
    }
    if ((u8)(code - 0x5B) < 0x36) {
        return (u8)(code + 0xCC);
    }
    if ((u8)(code + 0x66) < 3) {
        return (u8)(code + 0x8A);
    }
    return 1;
}
