#include "common/types.h"

s32 func_0025FD00(u16 value)
{
    if (value < 8) {
        return 1;
    }
    if ((u16)(value - 8) < 8) {
        return 2;
    }
    if ((u16)(value - 0x10) < 8) {
        return 3;
    }
    if ((u16)(value - 0x18) < 8) {
        return 4;
    }
    if ((u16)(value - 0x20) < 7) {
        return 5;
    }
    if ((u16)(value - 0x27) < 8) {
        return 6;
    }
    if ((u16)(value - 0x2F) < 4) {
        return 7;
    }
    return 0;
}
