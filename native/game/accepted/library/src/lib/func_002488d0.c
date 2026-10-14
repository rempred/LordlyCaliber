#include "common/types.h"

typedef struct LinkGroupRecord {
    u8 bytes[0x19];
} LinkGroupRecord;

extern LinkGroupRecord D_801971F0[];
extern u8 D_80193BF3[][0x38];
extern void memset_00023780(void *destination, s32 size);
extern u8 func_8016BFC0(void);
extern u8 func_801D4880(u8 index);
extern void func_8016C150(u8 index);

s32 func_002488D0(u8 *staged, u32 selector)
{
    LinkGroupRecord *record;
    u32 i;

    if ((u8)selector == 0xFF) {
        for (i = 0; (u8)i < 30; i++) {
            if ((D_801971F0[(u8)i].bytes[1] & 1) == 0) {
                selector = i;
                break;
            }
        }
        if ((u8)i == 30) {
            return 1;
        }
    }
    record = &D_801971F0[(u8)selector];
    memset_00023780(record, 0x19);
    record->bytes[0] = func_8016BFC0();
    record->bytes[1] = 1;
    /* Keep the increments on their separate paths. GCC 2.7.2 otherwise
     * advances the wide counter in the first test's unconditional delay slot. */
    for (i = 0; (u8)i < 5;) {
        if (staged[(u8)i * 8] != 0) {
            if (staged[(u8)i * 8 + 1] != 0) {
                record->bytes[(u8)i + 2] = staged[(u8)i * 8];
                record->bytes[(u8)i + 7] = staged[(u8)i * 8 + 1] - 1;
            }
            i++;
        } else {
            i++;
        }
    }
    record->bytes[0x18] = func_801D4880(selector);
    D_80193BF3[record->bytes[2]][0] |= 2;
    func_8016C150(selector);
    return 0;
}
