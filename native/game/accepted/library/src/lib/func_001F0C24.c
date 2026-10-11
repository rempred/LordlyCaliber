#include "game/combat_types.h"

typedef unsigned char u8;

/* A view from each twelve-byte scan position, rather than a record stride.
 * The accessed cache fields begin 0xC0 bytes after that position.
 */
typedef struct {
    u8 prefix[0xC0];
    u8 *field_C0;
    u32 field_C4;
    u32 field_C8;
} CombatServiceView;

extern CombatServiceView *D_801CE8BC;
extern u8 *func_001F0A9C(u32);
extern int func_00201E08(int, int, int);
extern u32 func_00201E38(int, u32);
extern void *func_00001330(u32);

u8 *func_001F0C24(u32 key)
{
    /* Volatile access prevents KMC from hoisting the 0xC0 field offset into
     * the scan base. Read the found pointer before refreshing its counter.
     */
    volatile CombatServiceView *scan = D_801CE8BC;
    CombatServiceView *slot;
    u32 i, selected, row, x;
    u8 *found;
    u8 *source, *output, *sourceRow, *destination;

    for (i = 0; i < 16; scan = (CombatServiceView *)((u8 *)scan + 12)) {
        i++;
        if (scan->field_C0 != 0 && scan->field_C4 == key) {
            found = scan->field_C0;
            scan->field_C8 = 15;
            return found;
        }
    }
    /* Keep the same shift/add address expression at lookup and publication;
     * using a multiply changes KMC's address-operand order at lookup.
     */
    for (selected = 0; selected < 16; selected++) {
        if (((CombatServiceView *)((u8 *)D_801CE8BC +
                                  ((selected << 3) + (selected << 2))))->field_C0 == 0) {
            source = func_001F0A9C(0x00391270);
            output = func_00001330(func_00201E08(0, 90, 15) + 8);
            output[0] = 0x36;
            output[1] = 0x34;
            output[2] = 2;
            *(u16 *)(output + 4) = 90;
            output[3] = 0;
            *(u16 *)(output + 6) = 15;
            for (row = 0; row < 15; row++) {
                /* Unsigned arithmetic retains the low 32-bit products for
                 * every key. The two helpers remain calls on every row.
                 */
                sourceRow = source + (func_00201E38(4, 90) * (row + key * 15) + 8);
                destination = output + (func_00201E38(0, 90) * row + 8);
                for (x = 0; x < 90; x += 4) {
                    *destination++ = ((*sourceRow >> 2) & 0x30) | ((*sourceRow >> 4) & 3);
                    *destination++ = ((*sourceRow << 2) & 0x30) | (*sourceRow & 3);
                    sourceRow++;
                }
            }
            slot = (CombatServiceView *)((u8 *)D_801CE8BC +
                                        ((selected << 3) + (selected << 2)));
            slot->field_C0 = output;
            slot->field_C4 = key;
            slot->field_C8 = 15;
            return slot->field_C0;
        }
    }
    return 0;
}
