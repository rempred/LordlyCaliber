#include "game/effect_geometry_e05_internal.h"

extern u8 *D_801969B0;
void func_002673F0(E05Object2673F0 *object)
{
    func_800988A0(object->value_04, D_801969B0);
    COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
    if (object->value_0C) {
        COMBAT_DRAW_PAIR(0xE7000000, 0);
        COMBAT_DRAW_PAIR(0xDB06003C, (u32)object->value_0C - 0x80000000u);
        COMBAT_DRAW_PAIR(0xE7000000, 0);
        COMBAT_DRAW_PAIR(0xDE000000, (u32)object->value_08);
    }
    COMBAT_DRAW_PAIR(0xE7000000, 0);
    COMBAT_DRAW_PAIR(0xDB060038, (u32)object->value_14 - 0x80000000u);
    COMBAT_DRAW_PAIR(0xE7000000, 0);
    COMBAT_DRAW_PAIR(0xDE000000, 0x0E000000);
    COMBAT_DRAW_PAIR(0xD8380002, 64);
    D_801969B0 += 64;
}
