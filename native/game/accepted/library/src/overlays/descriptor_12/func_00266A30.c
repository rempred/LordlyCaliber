#include "game/effect_geometry_e05_internal.h"

extern u8 *D_801969B0;
extern u8 D_80220410[], D_80220E20[];
extern E05Point6S16 D_80220468;
extern float D_801D06FC;
void func_00266A30(E05Object2667C0 *object)
{
    E05Point6S16 points[128];
    float matrix[4][4];
    float origin[2];
    float projected[3];

    if (object->value_4A > 128) return;
    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_80220410);
    COMBAT_DRAW_PAIR(0xFA000000, (object->color[0] << 24) |
                     (object->color[1] << 16) | (object->color[2] << 8) |
                     object->color[3]);
    if (object->mode_44 == 0) {
        E05Point6S16 *cursor;
        float radiansPerDegree;
        float angle, radius, step;
        s32 i, count;

        func_80098B60(matrix, D_801D06FC, D_801D06FC, D_801D06FC);
        cursor = points;
        func_800989A0(object, matrix, matrix);
        func_800989A0(matrix, D_80220E20, matrix);
        radiansPerDegree = 0.017453292f;
        func_002638D0(matrix[3][0], matrix[3][1], matrix[3][2], &origin[0], &origin[1]);
        i = 0;
        func_80098C20(D_801969B0, origin[0], origin[1], 0.0f);
        COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
        angle = 0.0f;
        step = 360.0f / object->value_4A;
        radius = object->value_48;
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
            E05Point6S16 *point;

            for (j = 0, point = points; j < count; point++) {
                float x, y;

                func_80098450(matrix, point->x, point->y, 0.0f,
                             &projected[0], &projected[1], &projected[2]);
                func_002638D0(projected[0], projected[1], projected[2],
                              &projected[0], &projected[1]);
                x = projected[0] - origin[0];
                y = projected[1] - origin[1];
                j++;
                /* Direct s16 conversion preserves the pinned compiler's
                 * temporary allocation; an intermediate s32 changes it. */
                point->x = (s16)x;
                point->y = (s16)y;
            }
        }
        func_002664b0((struct E05Point6U16 *)(&D_80220468), (struct E05Point6U16 *)(points), count, object->plane_46);
        COMBAT_DRAW_PAIR(0xD8380002, 64);
        D_801969B0 += 64;
    }
}
