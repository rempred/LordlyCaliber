#include "common/types.h"
typedef float f32;
typedef double f64;
typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u8 *)(p) + (offset)))
extern s8 D_801F1038[];
extern s16 D_801F1040[];
extern u8 D_801F1051[];
extern u8 *D_801F361C;
extern Q7Unit *D_801F0D28[];
extern f64 D_801F0510;
extern f32 func_8009CFE0(f32, f32);

void func_0013A3D8(Q7Unit *unit, s32 arg1)
{
    u8 slot = arg1 & 0xF;
    s32 mode = arg1 & 0xF0;
    if (D_801F1038[slot] == -1) {
        u8 byte_value = D_801F361C[FIELD(unit, s32, 0x14)];
        u32 flags;
        Q7Unit *other;
        f32 first, second, value;
        D_801F1038[slot] = FIELD(unit, u8, 4);
        /* Keep separate stores: one conditional RHS makes KMC construct
         * a full table pointer early and adds an instruction. */
        if (mode)
            D_801F1040[slot] = 600;
        else
            D_801F1040[slot] = 300;
        D_801F1051[slot] = byte_value;
        flags = FIELD(unit, u32, 0);
        FIELD(unit, s32, 0x24) = 0;
        FIELD(unit, u8, 0x20) = 0;
        FIELD(unit, u32, 0) = (flags | 2) & ~0x800000;
        other = D_801F0D28[0];
        first = FIELD(other, f32, 0x10) - FIELD(unit, f32, 0x10);
        second = FIELD(other, f32, 8) - FIELD(unit, f32, 8);
        /* Divide in double precision, then round before the float updates. */
        value = (f32)((f64)func_8009CFE0(first, second) / D_801F0510);
        if (mode == 0)
            value += 0.5f;
        while (value < 0.0f)
            value += 1.0f;
        while (value >= 1.0f)
            value -= 1.0f;
        FIELD(unit, f32, 0x1C) = value;
    }
}


