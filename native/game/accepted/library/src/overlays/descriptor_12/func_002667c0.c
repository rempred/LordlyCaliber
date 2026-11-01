#include "game/effect_geometry_e05_internal.h"

extern u8 *D_801969B0;
extern u8 D_80220410[];
extern E05Point6S16 D_80220460;
void func_002667c0(E05Object2667C0 *object)
{
    E05Point6S16 points[128];

    if (object->value_4A > 128) return;
    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_80220410);
    func_800988A0(object, D_801969B0);
    COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
    COMBAT_DRAW_PAIR(0xFA000000, (object->color[0] << 24) |
                     (object->color[1] << 16) | (object->color[2] << 8) |
                     object->color[3]);
    if (object->mode_44 == 0) {
        float angle = 0.0f;
        float radius = object->value_48;
        float step = 360.0f / object->value_4A;
        E05Point6S16 *cursor = points;
        float radiansPerDegree = 0.017453292f;
        s32 i = 0;

        do {
            float radians = angle * radiansPerDegree;

            cursor->x = (s32)(radius * func_800907F0(radians));
            cursor->y = (s32)(radius * func_80092DB0(radians));
            cursor->value_04 = 255;
            i++;
            cursor++;
            angle += step;
        } while (angle < 360.0f);
        func_002664b0((struct E05Point6U16 *)(&D_80220460), (struct E05Point6U16 *)(points), i, object->plane_46);
    }
    COMBAT_DRAW_PAIR(0xD8380002, 64);
    D_801969B0 += 64;
}
