#include "common/types.h"

extern u16 D_80220E70[15];

void func_00260210(u16 *source)
{
    s32 i = 0;
    u16 *values = D_80220E70;
    u16 *current = values;
    u16 *record = values + 6;

    values[6] = source[0];
    for (; i < 4; i++) {
        record[5] = *current++;
        (record++)[1] = source[i + 1];
    }
    values[5] = 0;
    values[4] = 1;
}
