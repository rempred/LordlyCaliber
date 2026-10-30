#include "common/types.h"

void bzero(void *memory, s32 bytes);
extern u8 D_80220DB8[];

void func_00263190(void)
{
    *(s32 *)0x80220DB0 = 0;
    *(s32 *)0x80220E08 = 0;
    bzero(D_80220DB8, 0x50);
    *(s32 *)0x80221030 = 0;
}
