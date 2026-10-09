#include "game/combat_pose_pool.h"

void func_002071F4(void)
{
    u32 slot;
    u32 i;
    CombatPosePoolRecord *record;

    if (D_801D0728 != 0) {
        for (slot = 0; slot < 20; slot++) {
            record = &D_801D0728[slot];
            if (record->field_00 != 0) {
                for (i = 0; i < record->field_18; i++) {
                    if (record->field_38[i] != 0 && --record->field_60[i] == 0) {
                        func_000016C4(record->field_38[i]);
                        record->field_38[i] = 0;
                    }
                    if (record->field_3C[i] != 0 && --record->field_64[i] == 0) {
                        func_000016C4(record->field_3C[i]);
                        record->field_3C[i] = 0;
                    }
                    if (record->field_40[i] != 0 && --record->field_68[i] == 0) {
                        func_000016C4(record->field_40[i]);
                        record->field_40[i] = 0;
                    }
                }
                for (i = 0; i < 10; i++) {
                    /* The ten counters occupy bytes 0x98..0xA1 of the record. */
                    if (record->field_70[i] != 0 && --((u8 *)record + i)[0x98] == 0) {
                        func_000016C4(record->field_70[i]);
                        record->field_70[i] = 0;
                    }
                }
            }
        }
    }
}
