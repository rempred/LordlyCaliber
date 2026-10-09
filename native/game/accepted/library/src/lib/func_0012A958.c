typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))

extern u8 D_801971F0[][25];
extern u8 D_801969B8[][11];
extern u8 D_80193AC0[];
extern u16 D_8019532C[], D_80190EBC[];
extern RuntimeUnit *D_801F0CB0[];
extern u32 func_0014F300(s32 source_word);
extern s32 func_0012E968(RuntimeUnit *unit);
extern s32 func_0012E9F4(RuntimeUnit *unit);
extern s32 func_0010746C(void *unit);
extern u8 func_00129068(u8 *record);
extern s32 func_00040f88(u8, u8, u8, u8, u8);
extern s32 func_0012E8EC(RuntimeUnit *unit);
extern void func_00128E80(void);

void func_0012A958(RuntimeUnit *unit)
{
    u8 values[5];
    u8 *source_record;
    s32 slot;
    /* The member cleanup and item comparison reuse a word in disjoint phases.
     * Separate locals rotate the pinned compiler's first-loop temporaries. */
    s32 value;

    if ((FIELD(unit, u32, 0) & 0x00020000) != 0)
        FIELD(unit, u32, 0) &= ~0x00020000U;
    source_record = D_801971F0[FIELD(unit, u8, 4)];
    source_record[1] &= ~4;
    for (slot = 0; slot < 5; ++slot) {
        u8 *entry = source_record + slot;
        value = entry[2];
        if (value == 0)
            continue;
        if (value >= 100) {
            entry[2] = 0;
            if (FIELD(unit, u8, 4) >= 30)
                D_8019532C[value] = 0;
            else
                D_80190EBC[value] = 0;
        } else {
            if (FIELD(unit, u8, 4) >= 30) {
                u8 *class_row = (u8 *)0x80190000 + value * 52;
                *(u16 *)(class_row + 0x5578) = 0;
            } else {
                u8 *class_row = (u8 *)0x80190000 + value * 56;
                *(u16 *)(class_row + 0x3BD8) = 0;
            }
        }
    }
    if (FIELD(unit, u8, 4) < 30) {
        s32 count = (u16)func_0014F300(FIELD(unit, u8, 4));
        for (slot = 0; slot < count; ++slot) {
            u8 *carried = source_record + slot;
            s32 item = carried[13];
            if (item != 0) {
                u8 *inventory, *end;
                s32 offset;
                /* Keep the copied byte value before the pointer setup, then
                 * the offset before the end: this preserves the retail schedule. */
                value = item;
                inventory = D_80193AC0 + 2;
                offset = 0;
                end = inventory + 160;
                do {
                    if (*(u16 *)((u8 *)0x80190000 + offset + 0x3AC0) == value) {
                        if (*inventory != 0)
                            --*inventory;
                        break;
                    }
                    inventory += 4;
                    offset += 4;
                } while ((s32)inventory < (s32)end);
                {
                    u8 *clear_entry = source_record + slot;
                    clear_entry[13] = 0;
                }
            }
        }
    }
    if ((FIELD(unit, u32, 0) & 0x40) != 0) {
        s32 row_index = func_0012E968(unit);
        s32 selector = func_0012E9F4(unit);
        u8 *row = D_801969B8[row_index];
        if ((FIELD(unit, u32, 0) & 0x80) != 0) {
            row[1] &= ~5;
            D_801971F0[FIELD(unit, u8, 4)][1] &= ~2;
            for (slot = 0; slot < 5; ++slot) {
                s32 source = *(u8 *)((u32)row + slot + 2);
                RuntimeUnit *other;
                if (source == 0xFF)
                    continue;
                other = D_801F0CB0[source];
                FIELD(other, u32, 0) &= ~0x80U;
                FIELD(other, u32, 0) &= ~0x40U;
                FIELD(other, u32, 0) &= ~0x4000U;
                FIELD(other, u32, 0) &= ~0x8000U;
                FIELD(other, u32, 0) &= ~0x10000U;
                if ((FIELD(other, u32, 0) & 8) == 0) {
                    if (slot != 0)
                        FIELD(other, u32, 0) |= 0x20000000;
                } else {
                    func_0010746C(other);
                    FIELD(other, u32, 0) |= 0x00800000;
                    FIELD(other, f32, 0x4C) = FIELD(other, f32, 8);
                    FIELD(other, f32, 0x50) = FIELD(other, f32, 0xC);
                    FIELD(other, f32, 0x54) = FIELD(other, f32, 0x10);
                    FIELD(other, s32, 0x64) = FIELD(other, s32, 0x14);
                }
            }
        } else {
            switch (selector) {
            case 1:
                row[3] = row[5];
                row[5] = 0xFF;
                break;
            case 2:
                row[4] = row[6];
                row[6] = 0xFF;
                break;
            case 3:
                row[5] = 0xFF;
                break;
            case 4:
                row[6] = 0xFF;
                break;
            }
            FIELD(unit, u32, 0) &= ~0x40U;
            FIELD(unit, u32, 0) &= ~0x4000U;
            FIELD(unit, u32, 0) &= ~0x8000U;
            FIELD(unit, u32, 0) &= ~0x10000U;
            /* The classifier consumes five scratch bytes, not rewritten row IDs. */
            for (slot = 0; slot < 5; ++slot) {
                u8 *entry = (u8 *)((u32)row + slot);
                s32 source = entry[2];
                if (source == 0xFF)
                    values[slot] = 0;
                else
                    values[slot] = func_00129068(D_801971F0[source]);
            }
            row[9] = func_00040f88(values[0], values[1], values[2], values[3], values[4]);
            row[10] = 11 - func_0012E8EC(D_801F0CB0[row[2]]);
        }
    } else {
        FIELD(unit, u8, 0x91) = 0;
        FIELD(unit, u8, 0x92) = 0;
        FIELD(unit, u8, 0x90) = 0;
        FIELD(unit, u8, 0x98) = 0;
        FIELD(unit, f32, 0x9C) = -1.0f;
    }
    func_00128E80();
}
