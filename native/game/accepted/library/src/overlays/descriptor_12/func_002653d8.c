#include "game/effect_geometry_e05_internal.h"

extern CombatDrawVertex *D_80197178;
void func_002653d8(struct E05Descriptor *descriptor, E05Point8U16 *first,
                  E05Point8U16 *second, s32 count)
{
    struct {
        E05Point8U16 *points[4];
        CombatDrawVertex *cursor;
    } work;
    s32 i, j;

    /* Keeping the cursor with the point workspace retains its local home. */
    work.cursor = D_80197178;

    /* Evaluate the bound before incrementing i: KMC then hoists count - 1. */
    for (i = -1; count - 1 > ++i;) {
        s32 point;

        work.points[0] = second;
        work.points[1] = second + 1;
        work.points[2] = first + 1;
        work.points[3] = first;
        for (point = 0; point < 4; point++) {
            D_80197178->x = work.points[point]->value_00;
            D_80197178->y = work.points[point]->value_02;
            D_80197178->field_04 = 0;
            D_80197178->field_06 = 0;
            D_80197178->u = work.points[point]->value_04;
            D_80197178->v = work.points[point]->value_06;
            D_80197178->field_0C[0] = 0;
            D_80197178->field_0C[1] = 0;
            D_80197178->field_0C[2] = 0;
            D_80197178->field_0C[3] = 255;
            D_80197178++;
        }
        first++;
        second++;
    }
    count = (count - 1) * 4;
    while (count >= 4) {
        CombatDrawVertex *chunk;
        CombatDrawVertex *start;

        i = 32;
        if (count < 32) i = count;
        count -= i;
        start = work.cursor;
        COMBAT_DRAW_PAIR(0x01000000 | COMBAT_DRAW_FIELD(i, 12, 8) |
                         COMBAT_DRAW_FIELD(i, 1, 7), (u32)start);
        /* Snapshot the batch address before giving the call loop its cursor. */
        for (j = 0, chunk = start; j < i; j += 4, chunk += 4) {
            func_00265014(descriptor, (struct E05TextureVertex16 *)(chunk), j);
        }
        work.cursor = chunk;
    }
}
