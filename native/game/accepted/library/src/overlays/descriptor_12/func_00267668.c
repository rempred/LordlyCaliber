#include "game/effect_geometry_e05_internal.h"

extern u8 D_802205F0[];
s32 func_00267668(E05Object267668 *object)
{
    void *embedded = (u8 *)object + 0x18;

    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_802205F0);
    COMBAT_DRAW_PAIR(0xFA000000, COMBAT_DRAW_FIELD(object->value_30, 24, 8) |
                     COMBAT_DRAW_FIELD(object->value_32, 16, 8) |
                     COMBAT_DRAW_FIELD(object->value_34, 8, 8) |
                     ((u32)(object->value_1C->value_40 * 255.0f) & 255));
    func_002673F0((struct E05Object2673F0 *)(embedded));
    return 0;
}
