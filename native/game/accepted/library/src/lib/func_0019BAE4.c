#include "common/types.h"

void func_0019BAE4(s32 value, u8 *output)
{
    s32 quotient = value / 10;
    s32 remainder = value % 10;
    if (quotient != 0) {
        *output++ = quotient + 0x30;
    }
    output[0] = remainder + 0x30;
    output[1] = 0;
}
