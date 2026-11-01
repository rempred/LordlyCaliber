#include "game/effect_runtime_e03_internal.h"

typedef struct E03CommandPair {
    u16 mask_00;
    u16 command_02;
} E03CommandPair;

extern E03CommandPair D_80220240[3];

void func_00261F84(void)
{
    E03Pool94 *record;
    E03Pool94 *other;
    s32 count;
    s32 used;
    s32 index;
    s32 other_index;
    s32 pair_offset;
    u32 flags;

    count = D_80220D68;
    record = D_80220D6C;
    for (index = 0; index < count; index++, record++) {
        record->flags_0A &= 0xFFFE;
    }

    used = 0;
    record = D_80220D6C;
    /* Advancing the record first retains the original branch-delay scheduling. */
    for (index = 0; index < D_80220D68; record++, index++) {
        if (record->callback_04 != 0 && !(record->flags_0A & 1)) {
            D_80220D70[used] = record;
            used++;
            record->flags_0A |= 1;
            for (other_index = index + 1, other = record + 1;
                 other_index < D_80220D68; other_index++, other++) {
                if (other->callback_04 == record->callback_04) {
                    D_80220D70[used] = other;
                    used++;
                    other->flags_0A |= 1;
                }
            }
        }
    }
    if (used < D_80220D68) {
        D_80220D70[used] = 0;
    }

    for (index = 0; index < D_80220D68; index++) {
        record = D_80220D70[index];
        if (record == 0) {
            break;
        }
        flags = record->flags_08;
        if (flags & 0x8000) {
            if (flags & 0x4000) {
                record->callback_04(2, record);
                record->callback_04 = 0;
                func_002622B4(record);
            } else {
                record->callback_04(1, record);
                record->flags_08 |= 0x4000;
            }
        } else {
            for (pair_offset = 0; pair_offset < 3; pair_offset++) {
                if (D_80220240[pair_offset].mask_00 & flags) {
                    record->callback_04(D_80220240[pair_offset].command_02,
                                        record);
                }
            }
            if (!(record->flags_0A & 0x2000)) {
                record->field_10++;
                if (record->flags_0A & 0x0200) {
                    if (record->field_18 < record->field_10) {
                        record->field_10 = record->field_14;
                    }
                } else if (func_002623E0(record)) {
                    if (record->flags_0A & 0x8000) {
                        func_002623C4(record);
                    } else if (record->flags_0A & 0x4000) {
                        record->field_10 = 0;
                    } else {
                        record->field_10 = record->callback_04(6, record);
                    }
                }
            }
        }
    }
}
