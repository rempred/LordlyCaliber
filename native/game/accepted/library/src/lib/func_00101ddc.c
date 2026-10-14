#include "common/types.h"
typedef float f32;
typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u32)(p) + (offset)))

extern void func_8020CE00(void);
extern void func_80186190(s32, s32);
extern s32 func_80186210(s32);
extern void func_800EA6B0(void *, s32, s32);
extern void func_00048294(void);
extern void func_00131388(void *);
extern void func_00129978(s32);
extern void func_0012A050(void);
extern void memset_00023780(void *, s32);
extern void func_00113C28(void);
extern void *resource_alloc(u32);
extern void func_00105498(s32);
extern s32 func_00119454(void);
extern s32 func_0012DA9C(Q7Unit *);
extern s32 func_0012DA10(Q7Unit *);
extern s32 func_801DD2B0(Q7Unit *);
extern void *func_0012E7E8(Q7Unit *);
extern void func_0010746C(Q7Unit *);
extern s32 func_00022E60(void);
extern void func_00132B80(s32);
extern void func_00130E60(s32, s32);
extern s32 func_00130794(void);
extern s32 func_0012F530(Q7Unit *);
extern void *func_0002e138(u32);
extern void *boot_command_stream_dispatch(u32, ...);
extern void func_8020E9A4(void);
extern void *memcpy(void *, const void *, u32);
extern void func_00101CE0(void);

extern u8 D_8018F481, D_800E9C16, D_80193670;
extern s32 D_80186FF0, D_800EB0DC;
extern Q7Unit *D_801AB850, *D_801F0CB0[], *D_801F0D28, *D_801F0D2C;
extern u8 D_801E7E17[][12], D_801971F0[][25], D_801971F1[][25];
extern u8 D_800E7AC3, D_80196A34, D_80197AE7[], D_801939CC;
extern s32 D_800E7A90[];
extern u16 D_8019367C[], D_801EDB18[][4], D_801EDB1A[][4];
extern u8 D_801E7D60, D_801976D8, D_801976DC, D_801976E8;
extern s32 D_801F0D84, D_80214F64;
extern f32 D_801F3A38, D_801F0D94;
extern u8 D_801F0D90;
extern s32 D_801F0DA8, D_801F0DAC, D_801F0DB0;
extern s32 D_801F0DE0, D_801F0DE4, D_801F0DE8, D_801F0DEC;
extern s32 D_801F0DF0, D_801F0DF4, D_801F0DF8, D_801F0DFC;
extern s32 D_801F0E00, D_801F0E04;
extern u8 D_801F0E08, D_801F0E09, D_801F0E0A, D_801F0E0B;
extern u8 D_801F0E0C, D_801F0E0D, D_801F0E0E, D_801F0E0F;
extern u8 D_801F0E10, D_801F0E11, D_801F0E12, D_801F0E13;
extern u8 D_801F0E14, D_801F0E15, D_801F0E16, D_801F0E17, D_801F0E18;
extern u16 D_801F0EBA, D_801F0EBC;
extern u8 D_801F0FDE, D_801F0FDF, D_801F0FE0, D_801F0FE1, D_801F0FE2;
extern u8 D_801F1034, D_801F1035, D_801F1036, D_801F1037, D_801F1038;
extern s8 D_801F1039[];
extern u16 D_801F1042[], D_801F105C, D_801F1060[];
extern u8 D_801F1052[], D_801F105A, D_801F105E, D_801F105F, D_801F106C;
extern s32 D_801F3658, D_801F1074, D_801F36C4, D_801F36C8, D_801F36CC;
extern s32 D_801F0BA8, D_801F0BAC;
extern void *D_801F0BB0;
extern s16 D_801F36D0;
extern u16 D_801F36A2;
extern u8 D_801F365D, D_801F3674, D_801F3A34;
extern s8 D_801F3680;
extern s32 D_801F3610, D_801F39A8, D_801F3608, D_801F360C;
extern s32 D_801F0BC4, D_801F0BC8;
extern void *D_801F3620, *D_8018F500;
extern u8 D_8018F55A, D_8018F55B;
extern void *D_801F0B90, *D_801F36C0, *D_801F3614, *D_801F3654;
extern void *D_801F0BCC, *D_801F0B58, *D_801F0B5C, *D_801F0B60;
extern void *D_801F0B64, *D_801F0B68, *D_801F0B6C, *D_801F0B70, *D_801F0B74;
extern void *D_801F39A0, *D_801F4010, *D_801F0B54, *D_801F361C;
extern s32 D_801F3618, D_801F3660, D_801F0CA8;
extern u8 D_80214F68, D_801EB318[], D_801F36D8[];
extern u16 D_801F3706[][32];
extern u8 D_801E8240[], D_801E8241[], D_801E8242[], D_801E8243[];
extern u8 D_801F0BA0, D_801F0BA1, D_801F0BA2, D_801F0BA3;

void func_00101DDC(void)
{
    s32 index, state;
    /* Preserve the observed unused frame extent; its original local types/capacities are unknown. */
    volatile u32 unused_scratch[8];
    Q7Unit *unit, *other;
    s32 code, source_index, other_index;
    s32 blocked;

    func_8020CE00();
    D_801F0D84 = 2;
    D_80214F64 = 0;
    D_801F3A38 = 1.6f;
    func_80186190(0x21, 0);
    {
        s32 next = D_801E7E17[D_8018F481][0];
        s32 *current = &D_800EB0DC;
        if (*current != next + 1)
            func_800EA6B0((u8 *)current - 0x2C, next, 10);
    }
    func_00048294();
    for (index = 0; index < 50; ++index) {
        if (D_801971F1[index][0] & 1)
            func_00131388(D_801971F0[index]);
    }
    D_801F365D = 0;
    if (D_80186FF0 == -1) {
        func_00129978(-1);
        if (D_801F1034 != D_8018F481) {
            func_0012A050();
            D_80186FF0 = 0;
        }
    }
    func_00129978(D_80186FF0);
    if (D_80186FF0 == 0) {
        Q7Unit *current = D_801AB850;
        s32 reset_index = 0;
        u8 *reset_base = &D_801F0D90;
        u8 *selector;
        s32 empty_source = -1;
        u16 *slot_words;
        f32 saved_z;
        s32 saved_word;
        s32 selector_index;
        s32 negative_offset;
        u8 *selected_pointer;
        D_80196A34 = 0;
        *reset_base = FIELD(current, u8, 4);
        selector = &D_800E7AC3;
        saved_z = FIELD(current, f32, 0x10);
        slot_words = (u16 *)(reset_base + 0x2B2);
        selector_index = *selector;
        D_801F0DA8 = 64;
        D_801F0DAC = 64;
        /* Keep this as subtraction until combine; early PLUS reverses the pointer/index operands. */
        negative_offset = selector_index * -4;
        selected_pointer = (u8 *)((u32)selector - (u32)negative_offset);
        D_801F0D94 = saved_z;
        saved_word = FIELD(selected_pointer, s32, -0x33);
        D_801F0DE0 = 0;
        D_801F0DE4 = 0;
        D_801F0DE8 = 0;
        D_801F0DEC = 0;
        D_801F0DF0 = 0;
        D_801F0DF4 = 0;
        D_801F0DF8 = 0;
        D_801F0DFC = 0;
        D_801F0E00 = 0;
        D_801F0E04 = 0;
        D_801F0E08 = 0;
        D_801F0E09 = 0;
        D_801F0E0A = 0;
        D_801F0E0B = 0;
        D_801F0E0C = 0xFF;
        D_801F0E0D = 0xFF;
        D_801F0E0E = 0;
        D_801F0E0F = 0;
        D_801F0E10 = 0;
        D_801F0E11 = 0;
        D_801F0E12 = 0;
        D_801F0E13 = 0;
        D_801F0DB0 = saved_word;
        D_801F0E14 = 0;
        D_801F1034 = 0xFF;
        D_801F0E15 = 0;
        D_801F0E16 = 0;
        D_801F0E17 = 0;
        D_801F0EBA = 0;
        D_801F0FDF = 0;
        D_801F0FE0 = 0;
        D_801F0FE1 = 0;
        D_801F1035 = 1;
        D_801F0FE2 = 0;
        D_801F0E18 = 0;
        D_801F0EBA = 0;
        D_801F0EBC = 0;
        D_801F0FDE = 0;
        D_801F1036 = 0;
        D_801F1037 = 0;
        D_801F105A = 0;
        D_801F105C = 0;
        D_801F105E = 0;
        D_801F105F = 0;
        D_801F1038 = 0;
        for (reset_index = 0; reset_index < 8; ++reset_index) {
            D_801F1039[reset_index] = empty_source;
            *slot_words = 0xFFFF;
            D_801F1052[reset_index] = 0;
            ++slot_words;
        }
        for (reset_index = 5; reset_index >= 0; --reset_index)
            D_801F1060[reset_index] = 0;
        D_801939CC = 0xFF;
        D_801F106C = 0;
        memset_00023780(D_80197AE7, 8);
    }
    D_801F3658 = -1;
    D_801F1074 = -1;
    D_801F36C4 = -1;
    D_801F36C8 = -1;
    D_801F36CC = -1;
    D_801F0BA8 = -1;
    D_801F36D0 = -1;
    D_801F0BAC = 0;
    D_801F0BB0 = 0;
    D_801F3610 = 0;
    D_801F3680 = -1;
    D_801F3674 = 0;
    D_801F39A8 = 0;
    D_801F3A34 = 0;
    func_00113C28();
    D_801F3608 = 0x14A;
    D_801F360C = 15;
    D_801F0BC4 = -104;
    D_801F0BC8 = 131;
    D_801F3620 = resource_alloc(0x25800);
    D_8018F55A = 0;
    D_8018F55B = 0xFF;
    if (D_80186FF0 != 0) {
        FIELD(D_801AB850, u8, 4) = D_801F0D90;
        FIELD(D_801AB850, f32, 0x10) = D_801F0D94;
        FIELD(D_801AB850, s32, 0x14) = 0;
    }
    if (D_80186FF0 == 1 && D_80193670 == 0)
        D_80186FF0 = 3;
    func_00105498(0);
    state = D_80186FF0;
    if (state == 1) {
        if (func_00119454()) {
            unit = D_801F0CB0[D_801976DC];
            if (D_800E9C16 == 2) {
                D_801F0DE0 = 45;
                D_801F0DE8 = 0;
                D_801F365D |= 1;
                if (func_0012DA9C(unit))
                    D_801F36D0 = 0x187;
                else
                    D_801F36D0 = 0x155;
                D_8018F500 = func_0012E7E8(unit);
                D_801F3680 = 1;
            } else if (D_800E9C16 == 1) {
                D_801F0DE0 = 9;
                D_801F0DE8 = 0;
                D_801F365D |= 1;
                if (func_0012DA9C(unit))
                    D_801F36D0 = 0x34;
                else
                    D_801F36D0 = 0x24;
                D_8018F500 = func_0012E7E8(unit);
                D_801F3680 = -1;
                D_801F3674 = 2;
            }
            {
                u32 unit_flags;
                func_0010746C(unit);
                unit_flags = FIELD(unit, u32, 0);
                FIELD(unit, s32, 0x80) = -1;
                FIELD(unit, s32, 0x84) = -1;
                FIELD(unit, u8, 0x92) = 0;
                FIELD(unit, u8, 0x91) = 0;
                unit_flags &= ~0x800000U;
                unit_flags &= ~0x2000U;
                unit_flags |= 0x02000000;
                FIELD(unit, u32, 0) = unit_flags;
            }
            goto after_state;
        }
        source_index = D_801976E8;
        other_index = D_801976DC;
        unit = D_801F0CB0[source_index];
        code = D_8019367C[source_index];
        other = D_801F0CB0[other_index];
        if (D_801976D8 & 0x40) {
            u16 *row = D_801EDB18[0];
            u16 value;
            index = 0;
            if (*row == 0)
                goto table_end;
            /* The once-only scope preserves the pinned compiler's loop weighting
             * while the explicit backedge preserves the observed read/compare sequence. */
            do {
                /* The original has a separate first comparison read. */
                value = *(volatile u16 *)row;
    table_compare:
                if (value == source_index)
                    goto table_end;
                row += 4;
                value = *row;
                ++index;
                if (value != 0)
                    goto table_compare;
            } while (0);
table_end:;
            code = FIELD(D_801EDB1A, u16, (index * 4 + func_00022E60() % 3) * 2);
            if (!(D_80193670 & 0x10))
                goto reset_state;
            if (!func_0012DA10(other))
                goto reset_state;
            if (func_0012DA9C(other))
                goto reset_state;
            if (code == 0)
                goto reset_state;
            D_801F0DE0 = 30;
            D_801F0DE8 = -1;
            D_801F365D |= 1;
            if (D_800E9C16 == 2 && func_0012DA10(other) && !func_801DD2B0(other) &&
                !func_0012DA9C(other)) {
                D_801F0E00 = FIELD(other, u8, 4);
                D_8018F500 = func_0012E7E8(other);
                D_801F36D0 = 0x1A0;
                D_801F36A2 = code;
                D_801F365D |= 4;
            } else {
                func_00132B80(code);
            }
            func_00130E60(code, 0);
            D_801E7D60 = 0;
            goto after_state;
        } else if ((u32)(D_8018F481 - 45) < 2) {
            blocked = 0;
            if (!(FIELD(unit, u32, 0) & 0x10)) {
                u32 id = FIELD(unit, u8, 4);
                if (id == 30) {
                    u32 flag_bits = FIELD(D_801F0D2C, u32, 0) & 0x11;
                    blocked = flag_bits == 0;
                } else if (id == 31) {
                    if (!(FIELD(D_801F0D28, u32, 0) & 0x11))
                        blocked = 1;
                } else if (!func_0012DA10(other)) {
                    blocked = 1;
                } else if (func_0012DA9C(other)) {
                    blocked = 1;
                }
            } else {
                blocked = 1;
            }
            if (blocked || !(code & 0x7FFF))
                goto reset_state;
            if (func_00022E60() % 100 >= 80)
                goto reset_state;
            D_801F0DE0 = 30;
            D_801F0DE8 = -1;
            D_801F365D |= 1;
            if (D_800E9C16 == 2 && func_0012DA10(other) && !func_801DD2B0(other) &&
                !func_0012DA9C(other))
                goto regular_special;
            goto regular_fallback;
        } else {
            if (!func_0012DA10(other))
                goto reset_state;
            if (func_0012DA9C(other))
                goto reset_state;
            {
                s32 active = (((FIELD(unit, u32, 0) >> 4) ^ 1) & 1) & (code != 0);
                if (active == 0)
                    goto reset_state;
            }
            if (source_index == 30 || !(code & 0x7FFF))
                goto reset_state;
            if (!func_00130794() && func_00022E60() % 100 >= 80)
                goto reset_state;
            D_801F0DE8 = -1;
            D_801F0DE0 = 30;
            D_801F365D |= 1;
            if (D_800E9C16 == 2 && func_0012DA10(other) && !func_801DD2B0(other) &&
                !func_0012DA9C(other)) {
regular_special:
                D_801F0E00 = FIELD(other, u8, 4);
                D_8018F500 = func_0012E7E8(other);
                D_801F36D0 = 0x1A0;
                D_801F36A2 = code;
                D_801F365D |= 4;
            } else {
regular_fallback:
                func_00132B80(code);
            }
            func_00130E60(code, 0);
            D_8019367C[source_index] = 0;
            D_801E7D60 = 0;
            goto after_state;
        }
reset_state:
        D_801F0DE0 = 0;
        D_801F0DE8 = 0;
        D_801F365D &= 0xFE;
    } else if (state == 2) {
        D_801F0DE0 = 3;
        D_801F0FE0 = 1;
        D_801F0DE8 = 0;
        D_801F3658 = -1;
        D_801F365D |= 1;
    } else if (state == 3) {
        s32 *state_word = &D_801F0DE0;
        *state_word = 0;
        D_801F0DE8 = 0;
        if (func_80186210(0x1D)) {
            s32 selection = D_801976DC;
            s32 result;
            u32 post_index;
            Q7Unit *call_unit;
            /* Keep each publication phase ahead of its following indexed load. */
            do {
                *state_word = 4;
                D_801F0DE8 = 0;
            } while (0);
            call_unit = D_801F0CB0[selection];
            D_801F3658 = selection;
            D_801F365D |= 1;
            result = func_0012F530(call_unit);
            post_index = D_801F3658;
            do {
                D_801F0FE1 = result;
                D_801F0FDF = 5;
                D_801F0FE0 = 0;
            } while (0);
            post_index *= 4;
            FIELD(FIELD(D_801F0CB0, Q7Unit *, post_index), u32, 0) &= ~0x10000000U;
        }
    } else if (state == -1) {
        D_801F0DE0 = 0;
        D_801F0DE8 = 0;
    } else if (state == 5) {
        s32 selection = D_801976DC;
        Q7Unit *call_unit;
        do {
            D_801F0DE0 = 4;
            D_801F0DE8 = 0;
        } while (0);
        call_unit = D_801F0CB0[selection];
        D_801F3658 = selection;
        D_801F365D |= 1;
        D_801F0FE1 = func_0012F530(call_unit);
        D_801F0FDF = 5;
        D_801F0FE0 = 2;
    }
after_state:
    func_80186190(0x1D, 0);
    D_801F0B90 = func_0002e138(0x01DC873E);
    D_801F36C0 = func_0002e138(0x01DCB92C);
    D_801F3614 = func_0002e138(0x01DF7BF8);
    D_801F3654 = boot_command_stream_dispatch(0x0218E9B4, 25, -3, 0);
    D_801F0BCC = boot_command_stream_dispatch(0x0218E9B4, 25, -3, 5);
    D_801F0B58 = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 0);
    D_801F0B5C = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 1);
    D_801F0B60 = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 2);
    D_801F0B64 = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 3);
    D_801F0B68 = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 4);
    D_801F0B6C = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 5);
    D_801F0B70 = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 6);
    D_801F0B74 = boot_command_stream_dispatch(0x0218E9B4, 23, -3, 7);
    func_8020E9A4();
    D_80214F68 = 0;
    D_801F39A0 = boot_command_stream_dispatch(0x0218E9B4, 3, -3, 0);
    D_801F4010 = boot_command_stream_dispatch(0x0218E9B4, 26, -3, 0);
    D_801F0B54 = boot_command_stream_dispatch(0x0218E9B4, 26, -3, 1);
    boot_command_stream_dispatch(0x02093576, 2, -3, 0);
    boot_command_stream_dispatch(0x02093576, 2, -3, 6);
    boot_command_stream_dispatch(0x02093576, 2, -4, 2);
    boot_command_stream_dispatch(0x02093576, 3, -4, 0);
    {
        u32 context = D_8018F481;
        D_801F3618 = -1;
        D_801F3660 = -1;
        D_80186FF0 = 0;
        if ((context & 0xFF) == 0x27) {
            D_801F361C = boot_command_stream_dispatch(0x021AF9F8, 1, -3, 2);
        } else if ((u32)(context - 0x2F) < 2 || (u32)(context - 0x3C) < 2) {
            D_801F361C = boot_command_stream_dispatch(0x021AF9F8, 1, -3, 0);
        } else if ((context & 0xFF) == 0x33) {
            D_801F361C = boot_command_stream_dispatch(0x021AF9F8, 1, -3, 1);
        } else {
            D_801F361C = 0;
        }
    }
    memcpy(D_801EB318, D_801F36D8, 0x200);
    if (D_8018F481 == 0xD) {
        if (func_80186210(0xE)) {
            for (index = 7; index >= 0; --index)
                D_801F3706[index][0] = 0xFFFF;
        }
    } else if (D_8018F481 == 0x11) {
        if (func_80186210(0xF)) {
            for (index = 7; index >= 0; --index)
                D_801F3706[index][0] = 0xFFFF;
        }
    }
    func_00101CE0();
    {
        Q7Unit *current = D_801AB850;
        s32 column_offset = FIELD(current, u8, 4) * 4;
        s32 row_offset = D_8018F481 * 12;
        column_offset += row_offset;
        D_801F0BA0 = FIELD(D_801E8240, u8, column_offset);
        column_offset = FIELD(current, u8, 4) * 4;
        column_offset += row_offset;
        D_801F0BA1 = FIELD(D_801E8241, u8, column_offset);
        column_offset = FIELD(current, u8, 4) * 4;
        column_offset += row_offset;
        D_801F0BA2 = FIELD(D_801E8242, u8, column_offset);
        column_offset = FIELD(current, u8, 4) * 4;
        column_offset += row_offset;
        D_801F0BA3 = FIELD(D_801E8243, u8, column_offset);
    }
    D_801F0CA8 = 0;
}
