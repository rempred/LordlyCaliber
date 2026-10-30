#include "common/types.h"

extern u16 D_80220E70[15];

void func_00260094(s32 mode, u16 *source)
{
    u16 *values = D_80220E70;
    u16 *record;
    s32 i = 0;

    D_80220E70[5] = 0;
    D_80220E70[4] = mode;
    record = values + 6;
    values[6] = source[0];
    /* The record has a control halfword and two groups of four values. */
    for (; i < 4; i++) {
        record[5] = source[i + 1];
        /* Keep postincrement on the second store for the observed KMC schedule. */
        (record++)[1] = source[i + 5];
    }
}
