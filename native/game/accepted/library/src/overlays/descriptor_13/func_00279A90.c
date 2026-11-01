#include "game/effect_presentation_p06_internal.h"

typedef struct P06Vec79A90 { float x, y, z; } P06Vec79A90;
typedef struct P06Point79A90 { s16 x, y, z; } P06Point79A90;
typedef struct P06State79A90 {
    u16 value_00;
    u16 unknown_02;
    u16 value_04;
    P06Point79A90 value_06[12];
} P06State79A90;
typedef struct P06Offsets79A90 { u16 values[4]; } P06Offsets79A90;
extern const P06Offsets79A90 p06_offsets_80225880;
extern u32 func_00022e60(void);

u8 func_00279A90(P06Vec79A90 *first, P06Vec79A90 *second, P06State79A90 *state)
{
    P06Vec79A90 step;
    P06Offsets79A90 offsets = p06_offsets_80225880;
    s16 parity = state->value_00;
    s16 spacing = state->value_04;
    s16 count, i;
    P06Point79A90 *points = state->value_06;

    func_0024C7A4(&step, second, first);
    count = func_0025EF78(&step) / spacing;
    if (count >= 12)
        count = 11;
    step.x /= count;
    step.y /= count;
    step.z /= count;
    points[0].x = first->x;
    points[0].y = first->y;
    points[0].z = first->z;
    /* Keep the observed nonzero short-counter guard as well as the bound. */
    for (i = 1; i != 0; i++) {
        P06Point79A90 *point;
        if (i >= count)
            break;
        /* Keep the byte displacement before the base in the address producer. */
        point = (P06Point79A90 *)((u32)(i * sizeof(*point)) + (u32)points);
        point->x = first->x + step.x * i;
        point->y = first->y + step.y * i;
        point->z = first->z + step.z * i;
        if ((s16)(i % 2) == parity)
            point->z += offsets.values[(s32)func_00022e60() % 2];
    }
    points[count].x = second->x;
    points[count].y = second->y;
    points[count].z = second->z;
    if (count <= 0)
        count = 1;
    return count;
}
