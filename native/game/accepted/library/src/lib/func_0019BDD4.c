#include "common/types.h"

extern u8 func_001977E0(void);
/* The caller transports one pointer word and consumes only the returned byte. */
extern u32 func_801DADF0(void *record);
extern u32 D_801F3658;
extern s32 D_801F0DE0;
extern u8 D_801F0FE1, D_801F0FDF, D_801F0FE0;

void func_0019BDD4(void)
{
    if (func_001977E0() == 0) {
        u32 index = D_801F3658;
        D_801F0DE0 = 4;
        /* Original pointer-word base is 0x130 below this state word.
         * Use word-address arithmetic without asserting an array extent. */
        D_801F0FE1 = func_801DADF0(
            *(void **)((u32)&D_801F0DE0 - 0x130 + index * 4));
        D_801F0FDF = 5;
        D_801F0FE0 = 2;
    }
}
