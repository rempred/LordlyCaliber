#include "common/types.h"
typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u32)(p) + (offset)))
extern u16 D_80196A2C;
extern u8 D_80193BC0[], D_80195560[];
extern u8 D_801971F0[], D_801971F2[], D_801969B8[];
extern u8 D_80193BD8[], D_80195578[];
extern Q7Unit *D_801F0CB0[];
extern u8 func_00129068(u8 *);
extern u8 func_001291B4(u8 *);
extern void func_0010746C(Q7Unit *);
extern void func_801F6C48(Q7Unit *);

static __inline__ s32 value_present(Q7Unit *unit)
{
    u32 source_index = FIELD(unit, u8, 4);
    s32 value = D_801971F2[source_index * 25];
    s32 result = 0;
    if ((value == 0) | (value >= 100))
        return 0;
    if (source_index < 30)
        result = FIELD(D_80193BD8 + value * 56, u16, 0) != 0;
    else
        result = FIELD(D_80195578 + value * 52, u16, 0) != 0;
    return result;
}

static __inline__ s32 flag_present(Q7Unit *unit)
{
    u32 source_index = FIELD(unit, u8, 4);
    /* Keep this query independent through the existing base-plus-two view. */
    s32 value = D_801971F0[source_index * 25 + 2];
    s32 result = 0;
    if ((value == 0) | (value >= 100))
        return 0;
    {
        s32 flags;
        if (source_index < 30)
            flags = FIELD(D_80193BD8 + value * 56, u8, 0x1B) & 4;
        else
            flags = FIELD(D_80195578 + value * 52, u8, 0x1B) & 4;
        result = flags != 0;
    }
    return result;
}
void func_0012FE58(s32 mode)
{
    u8 old_values[2];
    u8 new_values[2];
    s32 entry, side, index, slot, group_index;
    u8 *record, *source, *cursor, *group;
    Q7Unit *unit;
    /* Real scalar phases keep the first reset sentinel and later record
     * address in one saved register; a pointer-copy alias loses that allocation. */
    u32 word_work;

    if (mode != 0) {
        if (mode == 1) {
            old_values[0] = 0x60;
            new_values[0] = 0x5F;
            old_values[1] = 0x2B;
            new_values[1] = 0x2C;
        } else if (mode == 3) {
            old_values[0] = 0x60;
            new_values[0] = 0x5F;
            old_values[1] = 0x2C;
            new_values[1] = 0x2B;
        } else if ((u32)(D_80196A2C - 6) < 12) {
            old_values[0] = 0x60;
            new_values[0] = 0x5F;
            old_values[1] = 0x2B;
            new_values[1] = 0x2C;
        } else {
            old_values[0] = 0x5F;
            new_values[0] = 0x60;
            old_values[1] = 0x2C;
            new_values[1] = 0x2B;
        }
    } else {
        old_values[0] = 0x5F;
        new_values[0] = 0x60;
        old_values[1] = 0x2C;
        new_values[1] = 0x2B;
    }
    entry = 1;
    word_work = (u32)-1;
    for (; entry < 100; ++entry) {
        record = D_80193BC0 + entry * 56;
        for (side = 0; side < 2; ++side) {
            if (!record[0x11])
                continue;
            if (record[0x12] != old_values[side])
                continue;
            record[0x12] = new_values[side];
            if (side == 0)
                record[0x11] = new_values[0];
            index = 0;
            cursor = D_801971F0;
            for (; index < 30; ++index, cursor += 25) {
                source = cursor;
                slot = 0;
                if (source[1]) {
                    for (; slot < 5; ++slot) {
                        if (FIELD(source, u8, slot + 2) == entry)
                            break;
                    }
                    if (slot != 5)
                        break;
                }
            }
            if (index == 30)
                continue;
            unit = D_801F0CB0[index];
            source[0x18] = func_00129068(source);
            if (mode < 3)
                FIELD(unit, u32, 0x70) = source[0x18];
            group_index = 0;
            { u8 *group_cursor = D_801969B8;
            for (; group_index < 10; ++group_index, group_cursor += 11) {
                group = group_cursor;
                slot = 0;
                if (group[1]) {
                    for (; slot < 5; ++slot) {
                        if (FIELD(group, u8, slot + 2) == index)
                            break;
                    }
                    if (slot != 5)
                        break;
                }
            }
            }
            if (group_index != 10)
                group[9] = func_001291B4(group);
            if (mode == 3)
                continue;
            if (!(source[1] & 4))
                continue;
            {
            s32 side_active = side != 0;
            u32 flags = FIELD(unit, u32, 0);
            if (!(flags & 0x40)) {
                /* An explicit result preserves the bitwise test before inversion. */
                s32 active = (side_active != 0) & (mode != 0);
                if (active == 0)
                    goto secondary_mode;
                goto reset_primary;
            } else if ((side_active != 0) & (mode != 0)) {
                s32 member_index = 0;
                if (flags & 0x80) {
reset_primary:
                    func_0010746C(unit);
                    FIELD(unit, u32, 0x24) = 0;
                    FIELD(unit, u8, 0x92) = 0;
                    FIELD(unit, u8, 0x91) = 0;
                    FIELD(unit, s32, 0x80) = word_work;
                    FIELD(unit, s32, 0x84) = word_work;
                    FIELD(unit, u32, 0) &= ~0x2000U;
                    if (mode < 2)
                        FIELD(unit, u32, 0x78) |= 0x80000;
                    if (FIELD(unit, u32, 0) & 0x1000) {
                        FIELD(unit, u32, 0) &= ~0x1000U;
                        FIELD(unit, u32, 0xAC) = 0;
                        FIELD(unit, u32, 0xB0) = 0;
                        FIELD(unit, u8, 0xB4) = 0;
                    }
                } else {
                    u32 sentinel = 0xFF;
                    for (; member_index < 5; ++member_index) {
                        u32 source_index = FIELD(group, u8, member_index + 2);
                        if (source_index != sentinel) {
                            Q7Unit *member = D_801F0CB0[source_index];
                            func_0010746C(member);
                            FIELD(member, u32, 0x24) = 0;
                            FIELD(member, u8, 0x92) = 0;
                            FIELD(member, u8, 0x91) = 0;
                            FIELD(member, s32, 0x80) = word_work;
                            FIELD(member, s32, 0x84) = word_work;
                            if (FIELD(member, u32, 0) & 0x1000) {
                                FIELD(member, u32, 0) &= ~0x1000U;
                                FIELD(member, u32, 0xB0) = 0;
                                FIELD(member, u8, 0xB4) = 0;
                            }
                        }
                    }
                    if (mode < 2)
                        FIELD(unit, u32, 0x78) |= 0x80000;
                }
                goto after_primary;
            }
secondary_mode:
            if ((side_active != 0) & (mode == 0)) {
                FIELD(unit, u32, 0x78) |= 0x02000000;
            }
after_primary:;
            }
            if (mode < 2) {
                if (FIELD(unit, u32, 0) & 0x10) {
                    func_801F6C48(unit);
                    if ((!(FIELD(unit, u32, 0) & 0x40)) & (mode == 0)) {
                        if (!value_present(unit) || flag_present(unit)) {
                            func_0010746C(unit);
                            FIELD(unit, u8, 0x92) = 0;
                            FIELD(unit, u8, 0x91) = 0;
                            FIELD(unit, s32, 0x80) = word_work;
                            FIELD(unit, s32, 0x84) = word_work;
                            FIELD(unit, u32, 0) &= ~0x800000U;
                            FIELD(unit, u32, 0) &= ~0x2000U;
                            FIELD(unit, u32, 0) |= 0x02000000;
                        }
                    }
                }
            }
        }
    }
    if (mode == 3)
        return;
    for (entry = 1; entry < 100; ++entry) {
        word_work = (u32)(D_80195560 + entry * 52);
        for (side = 0; side < 2; ++side) {
            if (!FIELD(word_work, u8, 0x11))
                continue;
            if (FIELD(word_work, u8, 0x12) != old_values[side])
                continue;
            if (FIELD(word_work, u8, 0x11) == FIELD(word_work, u8, 0x12))
                FIELD(word_work, u8, 0x11) = new_values[side];
            FIELD(word_work, u8, 0x12) = new_values[side];
            index = 30;
            /* Indexed source form retains the observed base-plus-750 setup. */
            for (; index < 50; ++index) {
                source = D_801971F0 + index * 25;
                slot = 0;
                if (source[1]) {
                    for (; slot < 5; ++slot) {
                        if (FIELD(source, u8, slot + 2) == entry)
                            break;
                    }
                    if (slot != 5)
                        break;
                }
            }
            if (index == 50)
                continue;
            unit = D_801F0CB0[index];
            source[0x18] = func_00129068(source);
            FIELD(unit, u32, 0x70) = source[0x18];
            group_index = 0;
            { u8 *group_cursor = D_801969B8;
            for (; group_index < 10; ++group_index, group_cursor += 11) {
                group = group_cursor;
                slot = 0;
                if (group[1]) {
                    for (; slot < 5; ++slot) {
                        if (FIELD(group, u8, slot + 2) == index)
                            break;
                    }
                    if (slot != 5)
                        break;
                }
            }
            }
            if (group_index != 10)
                group[9] = func_001291B4(group);
            if (!(source[1] & 4))
                continue;
            if (mode < 2) {
                if (FIELD(unit, u32, 0) & 0x10)
                    func_801F6C48(unit);
            }
        }
    }
}