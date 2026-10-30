#include "common/types.h"

extern void *func_80070F30(s32 size);
extern void **D_80220F20;
extern s32 D_80220D48;
extern void *D_80220D44;
extern u16 D_80220D40;

void func_00260DA0(void)
{
    D_80220F20 = func_80070F30(0x8000);
    D_80220D48 = 0;
    D_80220D44 = 0;
    D_80220D40 = 0;
}
