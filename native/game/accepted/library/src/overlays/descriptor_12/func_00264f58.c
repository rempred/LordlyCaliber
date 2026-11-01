#include "game/effect_geometry_e05_internal.h"

s32 func_00264f58(u8 *input)
{
    void *object = input + 0x18;
    func_002639E0((struct E05DrawObject *)(object));
    COMBAT_DRAW_PAIR(0xD9FFFFFF, 0x400);
    func_00264bb0((struct E05Object264BB0 *)(object));
    return 0;
}
