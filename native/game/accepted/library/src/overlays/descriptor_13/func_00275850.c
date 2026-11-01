#include "game/effect_presentation_p06_internal.h"

#include "game/combat_draw.h"

typedef struct P06Coordinate75850 { s16 value_00, value_02, value_04; } P06Coordinate75850;

typedef struct P06Record75850 {
    u16 value_00, value_02, value_04;
    u16 value_06[37];
} P06Record75850;
typedef struct P06State75850 {
    u8 unknown_00[0xA];
    s16 value_0A;
    P06Record75850 *value_0C;
} P06State75850;

extern CombatDrawVertex *D_80197178;

/* Commit the next cursor after XYZ. Keep color stores through the global
 * cursor because their byte writes may alias it. A plain block preserves the
 * retail allocation; do/while (0) wrappers change the old compiler loop weights. */
#define P06_VERTEX(at, step, vx, vy, vz, vs, vt) { \
    CombatDrawVertex *vertex = (at); \
    vertex->x = (vx); \
    vertex->y = (vy); \
    vertex->field_04 = (vz); \
    if (step) D_80197178 = vertex; \
    vertex->field_06 = 0; \
    vertex->u = (vs); \
    vertex->v = (vt); \
    D_80197178->field_0C[0] = 255; \
    D_80197178->field_0C[1] = 255; \
    D_80197178->field_0C[2] = 255; \
    D_80197178->field_0C[3] = 255; \
}

CombatDrawVertex *func_00275850(P06State75850 *state, s16 recordIndex)
{
    s32 selected = recordIndex;
    P06Record75850 *records = state->value_0C;
    s32 offset = selected * 80;
    s16 width = *(u16 *)(offset + (u32)records + 2);
    s16 index = 0;
    CombatDrawVertex *first = D_80197178;
    u16 half = width / 2;
    s16 *positions;

    offset += 6;
    positions = (s16 *)((u32)records + offset);

    for (; (s16)index < state->value_0A * 3;) {
        P06Coordinate75850 *previous;
        s32 a, b, c, d;
        s32 direction = positions[(s16)index + 3] > positions[(s16)index] ? -1 : 1;

        if (direction == 1) {
            a = 0;
            b = 0;
            c = 15;
            d = 23;
        } else {
            a = 15;
            b = 23;
            c = 0;
            d = 0;
        }
        index += 3;
        {
            CombatDrawVertex *output = D_80197178;
            P06Coordinate75850 *current = (P06Coordinate75850 *)((s16)index * 2 + (u32)positions);
            P06_VERTEX(output, 0, current->value_00, current->value_02 + 2,
                       current->value_04 - half, a << 7, b << 7);
            P06_VERTEX(D_80197178 + 1, 1, current->value_00, current->value_02 - 2,
                       current->value_04 + half, c << 7,
                       (previous = (P06Coordinate75850 *)((s16)(index -= 3) * 2 + (u32)positions), b << 7));
        }
        P06_VERTEX(D_80197178 + 1, 1, previous->value_00, previous->value_02 - 2,
                   previous->value_04 + half, c << 7, d << 7);
        P06_VERTEX(D_80197178 + 1, 1, previous->value_00, previous->value_02 + 2,
                   previous->value_04 - half, a << 7, d << 7);
        D_80197178++;
        index += 3;
    }
    return first;
}

#undef P06_VERTEX
