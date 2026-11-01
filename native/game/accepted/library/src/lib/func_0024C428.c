#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

extern s32 (*D_80209700[])(u8);
extern s32 func_0020D654(s32, s32);

static __inline__ u8 p05_index_kind(u8 index)
{
    if (index < 20) return 0;
    if ((u8)(index - 20) < 27) return 1;
    if ((u8)(index - 47) < 40) return 2;
    return 0xFF;
}

void func_0024C428(u8 *output, u8 index)
{
    s16 i;
    s16 count;
    s32 base;
    u8 position;
    u8 kind;

    kind = p05_index_kind(index);
    base = ((u32)~D_80209700[kind](index) >> 31) * 18 + 20;
    i = 0;
    count = 0;
    do {
        position = i + base;
        if (func_0020D654((position - 20) % 3, (position - 20) / 3) == -1) {
            output[count++] = position;
        }
    } while (++i < 9);
    output[count] = 0xFF;
}
