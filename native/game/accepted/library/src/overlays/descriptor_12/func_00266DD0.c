#include "game/effect_geometry_e05_internal.h"

extern u8 *D_801969B0;
extern u8 D_80220470[];
void func_00266DD0(E05Object266DD0 *object)
{
    E05Point6S16 center;
    E05Point6S16 points[128];
    E05Point6S16 *cursor = points;
    float radiansPerDegree;
    float angle, radius, step;
    s32 i, count;

    if (object->value_4A > 128) return;
    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_80220470);
    COMBAT_DRAW_PAIR(0xFA000000, (object->color[0] << 24) |
                     (object->color[1] << 16) | (object->color[2] << 8));
    func_800988A0(object, D_801969B0);
    radiansPerDegree = 0.017453292f;
    i = 0;
    COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
    center.y = 0;
    center.x = 0;
    center.value_04 = object->value_44;
    angle = 0.0f;
    radius = object->value_48;
    step = 360.0f / object->value_4A;
    do {
        float radians = angle * radiansPerDegree;

        cursor->x = (s32)(radius * func_800907F0(radians));
        cursor->y = (s32)(radius * func_80092DB0(radians));
        cursor->value_04 = 255;
        i++;
        cursor++;
        angle += step;
    } while (angle < 360.0f);
    count = i;
    {
        s32 j;
        for (j = 0; j < count; j++) {
            points[j].value_04 = object->color[3];
        }
    }
    func_002664b0((struct E05Point6U16 *)(&center), (struct E05Point6U16 *)(points), count, object->plane_46);
    COMBAT_DRAW_PAIR(0xD8380002, 64);
    D_801969B0 += 64;
}
