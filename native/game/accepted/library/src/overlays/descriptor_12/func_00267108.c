#include "game/effect_geometry_e05_internal.h"

extern u8 *D_801969B0;
extern CombatDrawVertex *D_80197178;
extern u8 D_80220520[];
void func_00267108(E05Object267108 *object)
{
    s32 vertical, horizontal, mode, shade, i;
    CombatDrawVertex *start;

    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_80220520);
    func_800988A0(object, D_801969B0);
    COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
    COMBAT_DRAW_PAIR(0xFA000000, (u32)(object->value_40 * 255.0f) & 255);
    vertical = object->value_44;
    horizontal = object->value_46;
    start = D_80197178;
    shade = 255;
    mode = object->value_48;
    for (i = 0; i < 3; i++) {
        s32 x, z;

        if (i == 0) x = 0;
        else if (i == 1) x = horizontal;
        else x = -horizontal;
        func_80093380(D_80197178, 16);
        switch (mode) {
        case 0:
            if (i == 0) z = vertical;
            else z = -vertical;
            break;
        case 1:
            if (i == 0) {
                z = 0;
                shade = 128;
            } else {
                z = vertical * 2;
                shade = 0;
            }
            break;
        case 2:
            if (i == 0) z = -2 * vertical;
            else z = 0;
            break;
        default:
            for (;;) {}
        }
        D_80197178->x = x;
        D_80197178->field_04 = z;
        D_80197178->field_0C[0] = 255;
        D_80197178->field_0C[1] = shade;
        D_80197178->field_0C[2] = shade;
        D_80197178->field_0C[3] = 255;
        D_80197178++;
    }
    COMBAT_DRAW_PAIR(0x01003006, (u32)start);
    COMBAT_DRAW_PAIR(0x05000204, 0);
    COMBAT_DRAW_PAIR(0xD8380002, 64);
    D_801969B0 += 64;
}
