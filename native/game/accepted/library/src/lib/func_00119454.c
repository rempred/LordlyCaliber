#include "common/types.h"
typedef float f32;
typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u32)(p) + (offset)))
extern u8 D_801939CC, D_801976D8, D_801976E8, D_8018F481;
extern u8 D_801F365D, D_801974DF, D_80193672, D_80193673;
extern u16 D_801F1060[];
extern volatile s32 D_801F0E00, D_801F0E04;
extern Q7Unit *D_801F0CB0[], *D_801F0D28, *D_801F0D2C;
extern u8 D_801971F0[][25], D_801971F2[][25], D_80197208[][25];
extern u8 D_801969B8[][11], D_801969BA[][11], D_801969C1[][11], D_801969C2[][11];
extern u16 D_80195578[][26], D_80193BD8[][28], D_80193BD6[][28], D_8019532C[];
extern s8 D_801F1039[];
extern u16 D_801F1042[], D_801951CC[][18];
extern u8 D_801F1052[], D_800EB1F0[];
extern void func_80186190(s32, s32);
extern s32 func_80186210(s32);
extern void func_0010746C(Q7Unit *);
extern void func_801F69FC(Q7Unit *);
extern void func_00118D0C(Q7Unit *);
extern void func_0012A958(Q7Unit *);
extern s32 func_0012ECDC(s32);
extern s32 func_0012E968(Q7Unit *);
extern s32 func_0012F010(s32);
extern void func_800EA604(void *, s32);
extern void func_00127154(s32, s32, s32);
extern void func_00118E44(Q7Unit *, Q7Unit *);
extern s32 func_801DD2B0(Q7Unit *);
extern s32 func_0012DA9C(Q7Unit *);
extern s32 func_0012DA10(Q7Unit *);
extern void func_0012F140(Q7Unit *);
extern s32 func_001305B4(Q7Unit *);
extern void func_001191CC(s32);
extern u8 func_00129068(u8 *);
extern u8 func_001291B4(u8 *);
extern s32 func_0012E8EC(Q7Unit *);
extern s32 func_0012FBB8(s32, s32);

static __inline__ void clear_field_20000(Q7Unit *unit)
{
    if (FIELD(unit, u32, 0) & 0x40) {
        s32 row = func_0012E968(unit);
        u32 sentinel = 0xFF;
        Q7Unit **owners = D_801F0CB0;
        u32 mask = ~0x20000U;
        u8 *cursor = D_801969BA[row];
        u8 *end = cursor + 5;
        for (;;) {
            u32 source_index = *cursor;
            if (source_index != sentinel) {
                Q7Unit *member = owners[source_index];
                FIELD(member, u32, 0) &= mask;
            }
            ++cursor;
            if ((s32)cursor >= (s32)end)
                break;
        }
    } else {
        FIELD(unit, u32, 0) &= ~0x20000U;
    }
}

static __inline__ void refresh_metadata(Q7Unit *unit)
{
    u8 value = func_00129068(D_801971F0[FIELD(unit, u8, 4)]);
    D_80197208[FIELD(unit, u8, 4)][0] = value;
    value = D_80197208[FIELD(unit, u8, 4)][0];
    FIELD(unit, u32, 0x70) = value;
    if (FIELD(unit, u32, 0) & 0x40) {
        s32 row = func_0012E968(unit);
        D_801969C1[row][0] = func_001291B4(D_801969B8[row]);
        D_801969C2[row][0] = 11 - func_0012E8EC(unit);
    }
}

s32 func_00119454(void)
{
    s32 primary_result = 0;
    s32 separate_mode = 0;
    s32 index = 0;
    s32 result;
    s32 first_index, second_index;
    Q7Unit *first_unit, *second_unit;
    s32 secondary_result;
    s32 primary_filter, secondary_filter;
    u8 first_state;
    s32 second_state;
    u32 timer_fill = 0xA8;
    u16 *timers = D_801F1060;
    s32 bits = D_801939CC;
    for (; index < 6; ++index) {
        if (!((bits >> index) & 1)) {
            if (*timers == 0)
                *timers = timer_fill;
        }
        ++timers;
    }
    {
        u8 *mode_flags = &D_801976D8;
    /* These once-only phases retain the measured mode allocation weight without runtime loops. */
    do {
        if (*mode_flags & 0x40)
            separate_mode = 1;
    } while (0);
    if (*mode_flags & 0x20) {
        func_80186190(0x24, 1);
        *mode_flags &= 0xDF;
    } else {
        func_80186190(0x24, 0);
        D_801F365D &= 0xFD;
    }
    }
    /* Preserve the observed first/second snapshot read order with the pinned compiler. */
    first_index = D_801F0E00;
    second_index = D_801F0E04;
    second_unit = D_801F0D28;
    result = 0;
    first_unit = D_801F0CB0[first_index];
    if (!separate_mode)
        second_unit = D_801F0CB0[second_index];
    if (D_8018F481 == 0x27 || ((D_8018F481 == 0x2F) | (D_8018F481 == 0x3C)) ||
        D_8018F481 == 0x36)
        primary_result = 1;
    if ((u8)primary_result) {
        s32 index_matches = second_index == 0x1F;
        s32 source_matches = D_801976E8 == 0x1E;
        source_matches &= index_matches;
        if (source_matches) {
            second_unit = D_801F0D28;
            second_index = 0x1E;
            D_801F0E04 = second_index;
            func_0010746C(D_801F0D2C);
            FIELD(D_801F0D2C, u32, 0) &= ~1U;
            func_801F69FC(D_801F0D2C);
            func_00118D0C(D_801F0D2C);
            for (index = 0; index < 30; ++index) {
                Q7Unit *unit = D_801F0CB0[index];
                if ((FIELD(unit, u32, 0) & 0x215) == 0x15) {
                    if (FIELD(unit, s32, 0x80) == 0x1F) {
                        s32 byte_index;
                        FIELD(unit, s32, 0x80) = 0x1E;
                        for (byte_index = 0; byte_index < 3; ++byte_index) {
                            if (FIELD(unit, u8, 0xBD + byte_index) == 0x5F)
                                FIELD(unit, u8, 0xBD + byte_index) = 0x5E;
                        }
                    }
                }
            }
            func_0012A958(D_801F0D2C);
            FIELD(second_unit, u32, 0) |= 1;
            {
                u8 *flags = &D_801974DF;
                *flags |= 4;
            }
            FIELD(second_unit, u8, 0xBA) = 1;
            if (D_80195578[D_801971F2[second_index][0]][0]) {
                u8 *state = &D_80193672;
                FIELD(second_unit, u32, 0) |= 0x01000000;
                FIELD(second_unit, u32, 0) |= 0x80000000;
                *state &= 0xFE;
                D_80193673 = (D_80193673 | 1) & 0xFD;
            } else {
                u8 *state = &D_80193672;
                *state |= 1;
                D_80193673 = (D_80193673 & 0xFE) | 2;
            }
        }
    }
    first_state = D_80193672;
    second_state = D_80193673;
    primary_result = func_0012ECDC(first_index);
    if (primary_result == 0 && D_8018F481 == 0x3F) {
        u32 first = D_801971F2[first_index][0];
        primary_result = 1;
        D_80193BD8[first][0] = D_80193BD6[first][0];
    }
    secondary_result = 0;
    if (!separate_mode) {
        secondary_result = func_0012ECDC(second_index);
        if (secondary_result && D_8018F481 == 0x3B && (u32)(second_index - 31) < 4) {
            u32 first = D_801971F2[second_index][0];
            s32 missing;
            if (second_index >= 30)
                missing = D_80195578[first][0] == 0;
            else
                missing = D_80193BD8[first][0] == 0;
            if (missing) {
                missing &= -((D_80193673 & 2) != 0);
                if (missing) {
                    u16 *extra = D_8019532C;
                    u8 *cursor = D_801971F2[second_index];
                    u8 *end = cursor + 5;
                    do {
                        s32 member = *cursor;
                        if (member != 0) {
                            if (member < 100)
                                D_80195578[member][0] = 0;
                            else
                                extra[member] = 0;
                        }
                        ++cursor;
                    } while ((s32)cursor < (s32)end);
                    secondary_result = 0;
                }
            }
        }
    }
    FIELD(first_unit, u16, 0xB6) += 90;
    clear_field_20000(first_unit);
    if (!separate_mode) {
        FIELD(second_unit, u16, 0xB6) += 90;
        clear_field_20000(second_unit);
    }
    primary_filter = func_0012F010(first_index);
    if (!separate_mode)
        secondary_filter = func_0012F010(second_index);
    else
        secondary_filter = 0;
    if (primary_filter) {
        if (first_index < 30) {
            if (first_index != 0)
                FIELD(first_unit, u32, 0) |= 0x00200000;
        } else {
            primary_filter = 0;
            primary_result = 0;
        }
    }
    secondary_result &= -(secondary_filter == 0);
    if (separate_mode) {
        if (primary_result == 0) {
            func_0010746C(first_unit);
            FIELD(first_unit, u32, 0) &= ~0x10U;
            func_800EA604(D_800EB1F0, 0x289);
            func_00118D0C(first_unit);
            func_0012A958(first_unit);
            goto after_actions;
        } else if (primary_filter) {
            if (first_index != 0)
                FIELD(first_unit, u32, 0) |= 0x00200000;
        }
    } else {
        if (first_state & 1) {
            if (second_state & 1) {
                if ((primary_result != 0) & (secondary_result != 0))
                    goto paired_mode_one;
                if (primary_result)
                    goto transfer_pair;
                if (secondary_result) {
                    func_0010746C(first_unit);
            FIELD(first_unit, u32, 0) &= ~0x10U;
            func_800EA604(D_800EB1F0, 0x289);
            func_00118D0C(first_unit);
                    func_0012A958(first_unit);
            goto after_actions;
                }
                goto reset_both;
            } else {
                if ((primary_result != 0) & (secondary_result != 0)) {
                    s32 a = D_801F0E00;
                    s32 b = D_801F0E04;
                    func_00127154(a, b, 0);
                    goto after_actions;
                }
                if (primary_result)
                    goto transfer_pair;
                if (secondary_result) {
                    func_0010746C(first_unit);
            FIELD(first_unit, u32, 0) &= ~0x10U;
            func_800EA604(D_800EB1F0, 0x289);
            func_00118D0C(first_unit);
                    func_0012A958(first_unit);
            goto after_actions;
                }
                goto reset_both;
            }
        } else if (second_state & 1) {
            if ((primary_result != 0) & (secondary_result != 0)) {
                func_00127154(D_801F0E04, D_801F0E00, 0);
                goto after_actions;
            }
            if (primary_result)
                goto transfer_pair;
            if (secondary_result) {
                func_0010746C(first_unit);
            FIELD(first_unit, u32, 0) &= ~0x10U;
            func_800EA604(D_800EB1F0, 0x289);
            func_00118D0C(first_unit);
                func_0012A958(first_unit);
            goto after_actions;
            }
            goto reset_both;
        } else {
            if ((primary_result != 0) & (secondary_result != 0)) {
paired_mode_one:
                func_00127154(D_801F0E00, D_801F0E04, 1);
                goto after_actions;
            }
            if (primary_result) {
transfer_pair:
                func_00118E44(first_unit, second_unit);
                goto after_actions;
            }
            if (secondary_result) {
                func_0010746C(first_unit);
            FIELD(first_unit, u32, 0) &= ~0x10U;
            func_800EA604(D_800EB1F0, 0x289);
            func_00118D0C(first_unit);
                func_0012A958(first_unit);
            goto after_actions;
            }
reset_both:
            {
                u32 mask = ~0x10U;
                func_0010746C(first_unit);
                FIELD(first_unit, u32, 0) &= mask;
                func_00118D0C(first_unit);
                func_0012A958(first_unit);
                func_0010746C(second_unit);
                FIELD(second_unit, u32, 0) &= mask;
                func_800EA604(D_800EB1F0, 0x289);
                func_00118D0C(second_unit);
                func_0012A958(second_unit);
            }
        }
    }
after_actions:
    if ((primary_result != 0) & (primary_filter == 0)) {
        u32 first = D_801971F2[first_index][0];
        s32 missing;
        if (first_index >= 30)
            missing = D_80195578[first][0] == 0;
        else
            missing = D_80193BD8[first][0] == 0;
        if (missing) {
            missing &= -((D_80193672 & 2) != 0);
            if (missing) {
                result = 1;
                if (FIELD(first_unit, u32, 0) & 0x40) {
                result = 0;
                FIELD(first_unit, u32, 0x78) |= 0x8000;
                } else if (first_index == 0) {
                result = 0;
                } else if (D_8018F481 == 4) {
                result = func_801DD2B0(first_unit) == 0;
                }
                for (index = 0; index < 8; ++index) {
                if (D_801F1039[index] == FIELD(first_unit, u8, 4))
                break;
                }
                if (index != 8) {
                    D_801F1039[index] = -1;
                    D_801F1042[index] = 0xFFFF;
                    D_801F1052[index] = 0;
                }
                goto after_primary_adjustment;
            }
        }
    }
    if (primary_result) {
        if (func_0012DA9C(D_801F0CB0[first_index]) && !primary_filter && (first_state & 4)) {
            result = 1;
            if (FIELD(first_unit, u32, 0) & 0x40) {
            result = 0;
            FIELD(first_unit, u32, 0x78) |= 0x8000;
            } else if (first_index == 0) {
            result = 0;
            } else if (D_8018F481 == 4) {
            result = func_801DD2B0(first_unit) == 0;
            }
            for (index = 0; index < 8; ++index) {
            if (D_801F1039[index] == FIELD(first_unit, u8, 4))
            break;
            }
            if (index != 8) {
                D_801F1039[index] = -1;
                D_801F1042[index] = 0xFFFF;
                D_801F1052[index] = 0;
            }
        } else if (primary_result) {
            Q7Unit **owner_base = D_801F0CB0;
            Q7Unit **owner;
            /* Keep the scaled index in the address phase; a direct array address reverses the temporary allocation. */
            do {
                u32 owner_offset = first_index * 4;
                owner = (Q7Unit **)(owner_offset + (u32)owner_base);
            } while (0);
            if (func_0012DA10(*owner)) {
                if (!func_0012DA9C(*owner) && (first_state & 8)) {
                    Q7Unit *unit = *owner;
                    u32 flags;
                    f32 x, y, z;
                    s32 value;
                    func_0010746C(unit);
                    flags = FIELD(unit, u32, 0);
                    x = FIELD(unit, f32, 8);
                    y = FIELD(unit, f32, 0xC);
                    z = FIELD(unit, f32, 0x10);
                    value = FIELD(unit, s32, 0x14);
                    FIELD(unit, s32, 0x80) = -1;
                    FIELD(unit, s32, 0x84) = -1;
                    FIELD(unit, u8, 0x92) = 0;
                    FIELD(unit, u8, 0x91) = 0;
                    FIELD(unit, u32, 0) = (flags | 0x800000) & ~0x2000U;
                    FIELD(unit, f32, 0x4C) = x;
                    FIELD(unit, f32, 0x50) = y;
                    FIELD(unit, f32, 0x54) = z;
                    FIELD(unit, s32, 0x64) = value;
                }
            }
        }
    }
after_primary_adjustment:
    if ((separate_mode == 0) & (secondary_result != 0)) {
        u32 first = D_801971F2[second_index][0];
        s32 missing;
        if (second_index >= 30)
            missing = D_80195578[first][0] == 0;
        else
            missing = D_80193BD8[first][0] == 0;
        if (missing)
            missing &= -((D_80193673 & 2) != 0);
        if (missing || func_0012DA9C(second_unit)) {
            func_0012F140(second_unit);
            if (!(FIELD(second_unit, u32, 0) & 0x1000))
                func_001305B4(second_unit);
        }
    }
    {
        u32 first = D_801971F2[first_index][0];
        s32 missing;
        if (first_index >= 30)
            missing = D_80195578[first][0] == 0;
        else
            missing = D_80193BD8[first][0] == 0;
        if (missing) {
            missing &= -((D_80193672 & 2) != 0);
            if (missing) {
                if (first_index == 0)
                    D_801F365D &= 0xFD;
            }
        }
    }
    func_001191CC(first_index);
    if (!separate_mode)
        func_001191CC(second_index);
    if ((FIELD(first_unit, u32, 0) & 0x11) == 0x11)
        refresh_metadata(first_unit);
    if (!separate_mode) {
        u32 flags = FIELD(second_unit, u32, 0);
        if (flags & 1) {
            if (flags & 0x10)
                refresh_metadata(second_unit);
        }
    }
    if ((u32)(D_8018F481 - 45) < 2) {
        s32 selector = 0;
        if (!separate_mode && !(FIELD(second_unit, u32, 0) & 0x10) &&
            FIELD(second_unit, u8, 4) == 30) {
            selector = -((FIELD(D_801F0D2C, u32, 0) & 0x11) == 0) & 5;
        } else if (!separate_mode && !(FIELD(second_unit, u32, 0) & 0x10) &&
                   FIELD(second_unit, u8, 4) == 31) {
            if (!(FIELD(D_801F0D28, u32, 0) & 0x11))
                selector = 6;
        }
        if (selector) {
            u16 flags;
            D_801F365D &= 0xFD;
            flags = D_801951CC[selector][0];
            D_801951CC[selector][0] = (flags & 0xFFFB) | 2;
            func_0012FBB8(first_index, selector);
        }
    } else if (!separate_mode && !(FIELD(second_unit, u32, 0) & 0x10) &&
               FIELD(second_unit, u8, 4) == 30) {
        D_801F365D &= 0xFD;
    }
    do {
    if (!func_80186210(0x26)) {
        s32 mode_matches = separate_mode == 0;
        s32 context_matches = D_8018F481 == 0xD;
        context_matches &= mode_matches;
        if (context_matches) {
            if (FIELD(second_unit, u8, 4) == 0x24) {
                if (FIELD(second_unit, u32, 0) & 0x01000000) {
                    func_80186190(0x26, 1);
                    func_80186190(0x25, 1);
                }
            }
        }
    }
    } while (0);
    return result;
}