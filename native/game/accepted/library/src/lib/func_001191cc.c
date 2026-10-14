#include "common/types.h"

typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u8 *)(p) + (offset)))
extern Q7Unit *D_801F0CB0[];
extern u8 D_801969BA[];
extern u8 D_801971F1[];
extern u8 D_801971F2[];
extern u8 D_801971FD[];
extern u16 D_80190F80[], D_801953F0[];
extern u8 D_80193AC2[], D_80193AC0[];
extern s32 func_0012E968(Q7Unit *);
extern void func_0012F8C0(Q7Unit *);
extern u32 func_0014F300(s32);
extern u16 func_8016B738(u16);

void func_001191CC(s32 source_index)
{
    /* A named base retains the native base-before-scaled-index construction. */
    Q7Unit **owners = D_801F0CB0;
    Q7Unit **owner = (Q7Unit **)(source_index * 4 + (u32)owners);
    u8 *rows = D_801969BA;
    s32 source_offset = source_index * 25;
    u8 *members = D_801971F2 + source_offset;
    u8 *cursor;
    s32 member_index;
    Q7Unit *unit;

    /* KMC transforms this fixed index loop into the observed pointer scan.
     * A hand-written pointer do/while changes the saved-register allocation. */
    for (member_index = 0; member_index < 5; ++member_index) {
        s32 member;
        cursor = members + member_index;
        member = *cursor;
        /* Combining these guards with && emits a different predicate. */
        if (member != 0) {
            if (member >= 100) {
                u16 *table = D_80190F80;
                u32 scaled_member;
                if (D_801971F1[source_offset] & 0x80)
                    table = D_801953F0;
                /* Retain a named scale for the native add operand order. */
                scaled_member = member * 2;
                if (*(u16 *)((u32)table + scaled_member - 0xC4) == 0) {
                    *cursor = 0;
                    unit = *owner;
                    if (FIELD(unit, u32, 0) & 0x80) {
                        s32 row = func_0012E968(unit);
                        s32 present = 0;
                        s32 high_members = 0;
                        u8 *other = members + 1;
                        u8 *slot = (u8 *)(row * 11 + (u32)rows) + 1;
                        u8 *limit = members + 5;
                        do {
                            present += *slot != 0xFF;
                            high_members += *other >= 100;
                            ++other;
                            ++slot;
                        } while ((s32)other < (s32)limit);
                        if (high_members < present) {
                            if (FIELD(unit, u32, 0) & 8) {
                                u32 flags = FIELD(unit, u32, 0x78);
                                if (!(flags & 0x8000))
                                    FIELD(unit, u32, 0x78) = flags | 0x40000;
                            } else {
                                func_0012F8C0(unit);
                            }
                        }
                    }
                }
            }
        }
    }

    {
        s32 offset = source_index * 25;
        if (!(D_801971F1[offset] & 0x80)) {
            s32 count = (u16)func_0014F300((u8)source_index);
            if (count < 10) {
                u8 *base = D_801971FD + offset;
                u8 *item = (u8 *)(count + (u32)base);
                u8 *end = base + 10;
                /* The explicit back edge keeps the table address local to
                 * each iteration across the query call. */
next_item:
                {
                    u32 id = *item;
                    if (id != 0) {
                        u16 index = func_8016B738(id);
                        /* Existing aliases name byte02 of one four-byte row.
                         * Separate spellings retain native load/store addresses. */
                        D_80193AC0[index * 4 + 2] = D_80193AC2[index * 4] - 1;
                        *item = 0;
                    }
                    ++item;
                }
                if ((s32)item < (s32)end)
                    goto next_item;
            }
        } else {
            s32 count = (u16)func_0014F300((u8)source_index);
            if (count < 10) {
                u8 *base = D_801971FD + offset;
                u8 *item = (u8 *)(count + (u32)base);
                u8 *end = base + 10;
                do {
                    *item++ = 0;
                } while ((s32)item < (s32)end);
            }
        }
    }
}
