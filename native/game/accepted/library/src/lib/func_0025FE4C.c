#include "common/types.h"

void bzero(void *memory, s32 bytes);
s32 func_001FFBFC(void);
s32 func_001FFCC8(void);
s32 func_001FFDA4(void);
extern u16 D_80220E70[15];

void func_0025FE4C(void)
{
    u16 *values = D_80220E70;

    bzero(values, 30);
    values[0] = (u8)func_001FFBFC();
    D_80220E70[1] = (u8)func_001FFCC8();
    D_80220E70[2] = (u8)func_001FFDA4();
    D_80220E70[3] = 0;
}
