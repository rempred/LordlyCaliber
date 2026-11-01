#include "game/effect_geometry_e05_internal.h"

extern CombatDrawVertex *D_80197178;

/* One logical body spans both original physical owners, including 266550. */
void func_002664b0(E05Point6U16 *first, E05Point6U16 *points, s32 count, s32 plane)
{
    CombatDrawVertex *cursor;
    s32 i, index;

    switch (plane) {
    case 0:
        D_80197178->x = first->x;
        D_80197178->y = first->y;
        D_80197178->field_04 = 0;
        break;
    case 1:
        D_80197178->x = first->x;
        D_80197178->y = 0;
        D_80197178->field_04 = first->y;
        break;
    case 2:
        D_80197178->x = 0;
        D_80197178->y = first->y;
        D_80197178->field_04 = first->x;
        break;
    default:
        for (;;) {}
    }
    D_80197178->u = 0;
    D_80197178->v = 0;
    D_80197178->field_0C[0] = 0;
    D_80197178->field_0C[1] = 0;
    D_80197178->field_0C[2] = 0;
    D_80197178->field_0C[3] = first->value_04;
    COMBAT_DRAW_PAIR(0x01001002, (u32)D_80197178);
    cursor = ++D_80197178;

    for (i = 0; i <= count; i++) {
        E05Point6U16 *point;

        index = i < count ? i : 0;
        /* Integer byte-address addition retains offset-before-base operand
         * order in the pinned compiler; typed pointer addition swaps it. */
        point = (E05Point6U16 *)((u32)(index * sizeof(*points)) + (u32)points);

        switch (plane) {
        case 0:
            D_80197178->x = point->x;
            D_80197178->y = point->y;
            D_80197178->field_04 = 0;
            break;
        case 1:
            D_80197178->x = point->x;
            D_80197178->y = 0;
            D_80197178->field_04 = point->y;
            break;
        case 2:
            D_80197178->x = 0;
            D_80197178->y = point->y;
            D_80197178->field_04 = point->x;
            break;
        default:
            for (;;) {}
        }
        D_80197178->u = 0;
        D_80197178->v = 0;
        D_80197178->field_0C[0] = 0;
        D_80197178->field_0C[1] = 0;
        D_80197178->field_0C[2] = 0;
        D_80197178->field_0C[3] = point->value_04;
        D_80197178++;
    }
    count++;
    while (count >= 2) {
        s32 nextCount;
        CombatDrawCommand *command;

        i = count;
        if (count >= 30) i = 30;
        /* Keep the command load ahead of the explicit next-count phase. */
        command = D_800E9BA0++;
        nextCount = count + 1;
        count = nextCount - i;
        index = i;
        command->first = 0x01000000 | COMBAT_DRAW_FIELD(index, 12, 8) |
                         COMBAT_DRAW_FIELD(index + 1, 1, 7);
        command->second = (u32)cursor;
        cursor += i - 1;
        while (index >= 2) {
            /* Both indices remain live while their triangle fields are built. */
            s32 nextIndex = index - 1;

            COMBAT_DRAW_PAIR((((u32)nextIndex << 9) & 0xFE00) |
                             COMBAT_DRAW_FIELD(index * 2, 0, 8) | 0x05000000, 0);
            index = nextIndex;
        }
    }
}
