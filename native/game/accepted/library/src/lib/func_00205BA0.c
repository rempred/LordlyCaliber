#include "game/combat_pose_pool.h"

#define RECORD_BYTES(token) (*(u8 *)(0x801CEEE0 + (token)))

void func_00205BA0(CombatPosePoolRecord *record)
{
    u32 *directory = record->field_44;
    u8 *cursor = (u8 *)directory + directory[0x98 / 4];
    u32 count = *cursor++;
    u32 index;

    record->field_AC = 0xFFFF;
    record->field_AE = 0xFFFF;
    record->field_A8 = 0xFFFF;
    record->field_AA = 0xFFFF;
    for (index = 0; index < count; index++) {
        if (*cursor == 1) {
            record->field_AA = record->field_A8;
            record->field_A8 = cursor[1];
        }
        cursor += RECORD_BYTES(*cursor);
    }
    if (record->field_4C >= 0x58) {
        cursor = (u8 *)directory + directory[0x160 / 4];
        count = *cursor++;
        for (index = 0; index < count; index++) {
            if (*cursor == 1) {
                record->field_AE = record->field_AC;
                record->field_AC = cursor[1];
            }
            cursor += RECORD_BYTES(*cursor);
        }
    }
}
