#include "common/types.h"
typedef float f32;
typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u32)(p) + (offset)))
extern s32 D_801F0DF8, D_801F0DE0, D_801F0DE8;
extern s32 D_801F3658, D_801F0BA8, D_801F0E04, D_801F0E00;
extern s32 *volatile D_801F0BB0;
extern Q7Unit *D_801F0CB0[];
extern u8 D_801F365D, D_801F105E, D_801F3680, D_801F3674;
extern u8 D_800E9C16;
extern u16 D_801F36D0;
extern s32 D_801F36C4[];
extern f32 D_801F0D98, D_801F0D9C, D_801F0DA0, D_801F0DA4;
extern void *D_8018F500;
extern u8 D_801971F2[], D_80193BD1[];
extern void func_0012EA80_row(s32, f32 *);
extern u32 func_801E5730(Q7Unit *);
extern void func_0011D8D4(s32);
extern void *func_0012E7E8(Q7Unit *);
extern void func_00131050(void);
extern s32 func_801DCF90(Q7Unit *);
extern void func_000ec838(s32, s32, s32, s32, s32, s32, s32, s32, s32);
extern void func_0013A3D8(Q7Unit *, s32);
extern void func_001072B8(Q7Unit *);
extern s32 func_0012E968(Q7Unit *);

void func_0011BB4C(void)
{
    s32 state = D_801F0DF8;
    Q7Unit *unit;
    Q7Unit *other;
    s32 *initial_pointer;
    s32 selected;
    s32 first_index;
    /* Retail reserves sixteen scratch bytes; only three coordinates are used. */
    f32 point[4];
    /* These exclusive paths share the real flag-mask and call-result phases.
     * Separate scalars change KMC GCC 2.7.2 register allocation. */
    s32 word_work;

    if (state == 0) {
        D_801F0DE0 = 1;
        D_801F365D &= 0xFE;
        return;
    }
    if (state != 1) {
        D_801F0DE0 = 8;
        return;
    }
    unit = D_801F0CB0[D_801F3658];
    if (D_801F105E == 0 && (first_index = D_801F0BA8) != -1) {
        f32 scaled0, scaled1;
        u8 byte_state;
        u32 coordinate_offset, byte_index;
        selected = first_index;
        func_0012EA80_row(first_index, point);
        if (FIELD(unit, u8, 0x20) == 0 && FIELD(unit, s32, 0x74) == selected) {
            if (!(FIELD(unit, u32, 0) & 0x40))
                goto reset_unit;
            goto bit40;
        }
        scaled0 = (point[2] - D_801F0D9C) * 64.0f / (D_801F0DA4 - D_801F0D9C);
        scaled1 = (point[0] - D_801F0D98) * 64.0f / (D_801F0DA0 - D_801F0D98);
        D_801F36C4[FIELD(unit, u8, 0x20)] = ((s32)scaled0 << 6) + (s32)scaled1;
        byte_index = FIELD(unit, u8, 0x20);
        FIELD(unit, u8, 0xBD + byte_index) = selected - 128;
        coordinate_offset = FIELD(unit, u8, 0x20) * 12;
        FIELD(unit, f32, 0x28 + coordinate_offset) = point[0];
        coordinate_offset = FIELD(unit, u8, 0x20) * 12;
        FIELD(unit, f32, 0x2C + coordinate_offset) = point[1];
        coordinate_offset = FIELD(unit, u8, 0x20) * 12;
        FIELD(unit, f32, 0x30 + coordinate_offset) = point[2];
        ++FIELD(unit, u8, 0x20);
        byte_state = D_801F365D;
        word_work = ~4;
        FIELD(unit, u32, 0) = (FIELD(unit, u32, 0) & word_work) | 0x200;
        D_801F0DE0 = 1;
        D_801F365D = byte_state & 0xFE;
        return;
    }

    /* Cache the initial view; the append path later rereads the pointer object. */
    initial_pointer = D_801F0BB0;
    other = D_801F0CB0[*initial_pointer];
    point[0] = FIELD(other, f32, 8);
    point[1] = FIELD(other, f32, 0x0C);
    point[2] = FIELD(other, f32, 0x10);
    if (*initial_pointer != D_801F3658)
        goto append_other;
    if (FIELD(unit, u32, 0) & 0x40)
        goto bit40;
    selected = (u8)func_801E5730(unit);
    if (selected == 0)
        goto reset_unit;
    func_0011D8D4(0);
    if (D_800E9C16 == 2) {
        u32 source_byte;
        D_801F0DE0 = 45;
        D_801F0DE8 = 0;
        D_801F36D0 = 366;
        D_8018F500 = func_0012E7E8(unit);
        D_801F3680 = 14;
        D_801F365D |= 1;
        source_byte = FIELD(unit, u8, 4);
        D_801F0E04 = selected;
        D_801F0E00 = source_byte;
        func_00131050();
        word_work = func_801DCF90(unit);
        func_000ec838(0, (u16)(D_801F36D0 + word_work), 160, 120, 160, 50,
                     D_80193BD1[D_801971F2[FIELD(unit, u8, 4) * 25] * 56], 0, 0);
        return;
    }
    if (D_800E9C16 == 1) {
        u32 source_byte;
        D_801F0DE0 = 9;
        D_801F0DE8 = 0;
        D_801F36D0 = 44;
        D_8018F500 = func_0012E7E8(unit);
        D_801F3680 = 14;
        D_801F3674 = 2;
        D_801F365D |= 1;
        source_byte = FIELD(unit, u8, 4);
        D_801F0E04 = selected;
        D_801F0E00 = source_byte;
        return;
    }
    func_0013A3D8(unit, FIELD(unit, u8, 0x7F));
    func_001072B8(unit);
    FIELD(unit, u8, 0x90) = 0;
    FIELD(unit, u8, 0x92) = 0;
    FIELD(unit, u8, 0x91) = 0;
    FIELD(unit, s32, 0x84) = -1;
    FIELD(unit, s32, 0x80) = -1;
    FIELD(unit, u32, 0) &= ~0x800000;
    return;

reset_unit:
    func_0011D8D4(0);
    return;

bit40:
    FIELD(unit, u8, 0xB5) = 0xFF;
    D_801F365D |= 1;
    D_801F0E00 = D_801F3658;
    D_801F0E04 = func_0012E968(unit);
    func_0011D8D4(0);
    return;

append_other:
    {
        f32 scaled0, scaled1;
        u32 byte_state;
        s32 picked;
        u32 coordinate_offset, byte_index;
        scaled0 = (FIELD(other, f32, 0x10) - D_801F0D9C) * 64.0f / (D_801F0DA4 - D_801F0D9C);
        scaled1 = (FIELD(other, f32, 8) - D_801F0D98) * 64.0f / (D_801F0DA0 - D_801F0D98);
        D_801F36C4[FIELD(unit, u8, 0x20)] = ((s32)scaled0 << 6) + (s32)scaled1;
        coordinate_offset = FIELD(unit, u8, 0x20) * 12;
        FIELD(unit, f32, 0x28 + coordinate_offset) = point[0];
        coordinate_offset = FIELD(unit, u8, 0x20) * 12;
        FIELD(unit, f32, 0x2C + coordinate_offset) = point[1];
        coordinate_offset = FIELD(unit, u8, 0x20) * 12;
        FIELD(unit, f32, 0x30 + coordinate_offset) = point[2];
        byte_index = FIELD(unit, u8, 0x20);
        FIELD(unit, u8, 0xBD + byte_index) = *D_801F0BB0 + 64;
        ++FIELD(unit, u8, 0x20);
        byte_state = D_801F365D;
        picked = *D_801F0BB0;
        FIELD(unit, u32, 0) = (FIELD(unit, u32, 0) | 4) & ~0x200;
        D_801F365D = byte_state & 0xFE;
        FIELD(unit, s32, 0x80) = picked;
        D_801F0DE0 = 1;
    }
}
