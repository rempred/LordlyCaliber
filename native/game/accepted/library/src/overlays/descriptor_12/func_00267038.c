#include "game/effect_geometry_e05_internal.h"

extern u8 *D_801969B0;
extern u8 D_802204C0[];
s32 func_00267038(E05Object267038 *object)
{
    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_802204C0);
    func_800988A0((u8 *)object + 0x14, D_801969B0);
    COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
    COMBAT_DRAW_PAIR(0xDE000000, (u32)object->value_54);
    COMBAT_DRAW_PAIR(0xD8380002, 64);
    D_801969B0 += 64;
    return 0;
}
