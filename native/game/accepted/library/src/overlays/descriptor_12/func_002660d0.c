#include "game/effect_geometry_e05_internal.h"

extern u16 D_80220400[];
extern u16 D_80220E70[15];
extern u8 D_80220350[], D_802203A0[], D_80220E20[];
extern u8 *D_801969B0;
extern float D_801D06FC;
void func_002660d0(E05Geometry2660D0 *geometry)
{
    E05FloatPoint16 points[2][128];
    float matrix[4][4];
    float origin[2];
    E05FloatPoint16 *buffers[2];
    s32 count;
    float angle;

    if (geometry->value_44 <= 0) return;
    if (geometry->value_46 > 128) return;
    if (geometry->value_48 > 2) return;
    if (geometry->descriptor.value_04 < 320) return;
    if (geometry->descriptor.value_06 < 240) return;
    count = geometry->value_44 / D_80220400[geometry->descriptor.value_0A] + 1;
    count += geometry->value_44 % D_80220400[geometry->descriptor.value_0A] != 0;
    if (count >= 128) return;

    if (geometry->value_4E) {
        COMBAT_DRAW_PAIR(0xDE000000, (u32)D_802203A0);
    } else {
        COMBAT_DRAW_PAIR(0xDE000000, (u32)D_80220350);
    }
    COMBAT_DRAW_PAIR(0xFA000000, COMBAT_DRAW_FIELD(D_80220E70[0], 24, 8) |
                     COMBAT_DRAW_FIELD(D_80220E70[1], 16, 8) |
                     COMBAT_DRAW_FIELD(D_80220E70[2], 8, 8) | 255);
    func_80098B60(matrix, D_801D06FC, D_801D06FC, D_801D06FC);
    func_800989A0(matrix, D_80220E20, matrix);
    func_800989A0(geometry, matrix, matrix);
    func_002638D0(matrix[3][0], matrix[3][1], matrix[3][2], &origin[0], &origin[1]);
    func_80098C20(D_801969B0, origin[0], origin[1], 0.0f);
    COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
    D_801969B0 += 64;
    if (geometry->value_42 > 0) {
        s32 step;
        s32 done, current;
        float angleStep;

        buffers[0] = points[0];
        buffers[1] = points[1];
        step = D_80220400[geometry->descriptor.value_0A];
        angle = 0.0f;
        func_00265d10((struct E05Geometry265D10 *)(geometry), angle, step, points[0], origin[0], origin[1], matrix);
        done = 0;
        current = 0;
        angleStep = 360.0f / geometry->value_46;
        do {
            s32 next;

            angle += angleStep;
            if (angle >= 360.0f) {
                angle = 0.0f;
                done = 1;
            }
            next = current ^ 1;
            func_00265d10((struct E05Geometry265D10 *)(geometry), angle, step, buffers[next], origin[0], origin[1], matrix);
            func_002655a0(&geometry->descriptor, (struct E05Point16U16 *)(buffers[current]), (struct E05Point16U16 *)(buffers[next]), count,
                          geometry->value_4E);
            current = next;
        } while (!done);
    }
    COMBAT_DRAW_PAIR(0xD8380002, 64);
}
