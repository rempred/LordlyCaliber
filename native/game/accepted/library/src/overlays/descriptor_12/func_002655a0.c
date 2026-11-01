#include "game/effect_geometry_e05_internal.h"

extern CombatDrawVertex *D_80197178;
void func_002655a0(struct E05Descriptor *descriptor, E05Point16U16 *first,
                  E05Point16U16 *second, s32 count, s32 emitField)
{
    E05Point16U16 *points[4];
    CombatDrawVertex *cursor = D_80197178;
    s32 i, j, ordinal;

    for (i = -1; count - 1 > ++i;) {
        s32 point;

        points[0] = &second[i];
        points[1] = &second[i + 1];
        points[2] = &first[i + 1];
        points[3] = &first[i];
        for (point = 0; point < 4; point++) {
            D_80197178->x = points[point]->value_00;
            D_80197178->y = points[point]->value_02;
            D_80197178->field_04 = 0;
            D_80197178->field_06 = 0;
            D_80197178->u = points[point]->value_04;
            D_80197178->v = points[point]->value_06;
            D_80197178->field_0C[0] = 0;
            D_80197178->field_0C[1] = 0;
            D_80197178->field_0C[2] = 0;
            D_80197178->field_0C[3] = 255;
            D_80197178++;
        }
    }
    count = (count - 1) * 4;
    ordinal = 0;
    while (count >= 4) {
        i = 32;
        if (count < 32) i = count;
        count -= i;
        COMBAT_DRAW_PAIR(0x01000000 | COMBAT_DRAW_FIELD(i, 12, 8) |
                         COMBAT_DRAW_FIELD(i, 1, 7), (u32)cursor);
        for (j = 0; j < i; j += 4) {
            if (emitField) {
                COMBAT_DRAW_PAIR(0xEE000000, first[ordinal++].value_0A << 16);
            }
            func_00265014(descriptor, (struct E05TextureVertex16 *)(cursor), j);
            cursor += 4;
        }
        ordinal++;
    }
}
