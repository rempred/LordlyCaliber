#include "game/effect_geometry_e05_internal.h"

extern u8 D_802205B0[];
s32 func_0026756C(u8 *object)
{
    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_802205B0);
    func_002673F0((struct E05Object2673F0 *)(object + 0x18));
    return 0;
}
