#include "common/types.h"
#include "game/effect_presentation_p04.h"

extern s32 (*D_80209700[])(u8);
extern u32 D_8020A2D4;
extern void *D_80209770[];
extern u16 D_801CEF10[];

static __inline__ u8 p04_index_kind(u8 index)
{
    if (index < 20) return 0;
    if ((u8)(index - 20) < 27) return 1;
    if ((u8)(index - 47) < 40) return 2;
    return 0xFF;
}

void func_0024E8DC(u8 index, s32 value)
{
    u32 complemented;

    complemented = ~D_80209700[p04_index_kind(index)](index);
    value = value * 2 + (complemented >> 31);
    func_00208DC8(D_80209770[D_8020A2D4], D_801CEF10[value]);
    if (++D_8020A2D4 >= 2) {
        D_8020A2D4 = 0;
    }
}
