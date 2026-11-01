#include "game/effect_adapters_e08.h"
#include "game/effect_presentation_p04.h"

extern s32 (*D_80209700[])(u8);
extern u16 D_801CEF10[];

static __inline__ u8 e08_index_kind(u8 index)
{
    if (index < 20) return 0;
    if ((u8)(index - 20) < 27) return 1;
    if ((u8)(index - 47) < 40) return 2;
    return 0xFF;
}

void func_0024E9BC(s32 argument, u8 index, s32 value)
{
    u32 complemented;

    complemented = ~D_80209700[e08_index_kind(index)](index);
    value = value * 2 + (complemented >> 31);
    func_00208DC8((void *)argument, D_801CEF10[value]);
}
