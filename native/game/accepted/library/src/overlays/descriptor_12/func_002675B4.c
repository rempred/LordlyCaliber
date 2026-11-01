#include "game/effect_geometry_e05_internal.h"

extern u8 D_802205D0[];
s32 func_002675B4(E05Object2675B4 *object)
{
    void *embedded = (u8 *)object + 0x18;

    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_802205D0);
    COMBAT_DRAW_PAIR(0xFA000000, (u32)(object->value_1C->value_40 * 255.0f) & 255);
    func_002673F0((struct E05Object2673F0 *)(embedded));
    return 0;
}
