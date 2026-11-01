#include "game/effect_geometry_e05_internal.h"

extern u16 D_802203F8[];
extern u8 *D_801969B0;
extern u8 D_80220350[];
extern u16 D_80220E70[15];
void func_00265a28(E05Geometry265A28 *geometry)
{
    E05Point8S16 points[2][128];
    E05Point8S16 *buffers[2];
    s32 count;

    if (geometry->value_44 <= 0) return;
    if (geometry->value_46 > 128) return;
    if (geometry->value_48 > 2) return;
    count = geometry->value_44 / D_802203F8[geometry->descriptor.value_0A] + 1;
    count += geometry->value_44 % D_802203F8[geometry->descriptor.value_0A] != 0;
    if (count >= 128) return;

    func_800988A0(geometry, D_801969B0);
    COMBAT_DRAW_PAIR(0xDA380000, (u32)D_801969B0);
    D_801969B0 += 64;
    COMBAT_DRAW_PAIR(0xDE000000, (u32)D_80220350);
    COMBAT_DRAW_PAIR(0xFA000000, COMBAT_DRAW_FIELD(D_80220E70[0], 24, 8) |
                     COMBAT_DRAW_FIELD(D_80220E70[1], 16, 8) |
                     COMBAT_DRAW_FIELD(D_80220E70[2], 8, 8) | 255);
    if (geometry->value_42 > 0) {
        s32 done;
        float angle;
        s32 step;
        s32 current;
        float angleStep;

        buffers[0] = points[0];
        buffers[1] = points[1];
        angle = 0.0f;
        step = D_802203F8[geometry->descriptor.value_0A];
        func_002657d0((struct E05Geometry2657D0 *)(geometry), angle, step, buffers[0]);
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
            func_002657d0((struct E05Geometry2657D0 *)(geometry), angle, step, buffers[next]);
            func_002653d8(&geometry->descriptor, (struct E05Point8U16 *)(buffers[current]), (struct E05Point8U16 *)(buffers[next]), count);
            current = next;
        } while (!done);
    }
    COMBAT_DRAW_PAIR(0xD8380002, 64);
}
