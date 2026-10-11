#include "game/combat_pose_record.h"

extern u32 *func_002060EC(int);

int func_00205608(int handle, int directoryIndex, u32 selected,
                  CombatPoseRecord *out)
{
    u32 *directory = func_002060EC(handle);
    u16 *record;
    int result;

    if (directory != 0) {
        record = (u16 *)((u32)directory + directory[directoryIndex]);
    } else {
        record = 0;
    }
    if (selected >= *record++) {
        result = 0;
    } else {
        record += selected * 8;
        out->field_00 = record[0];
        out->field_04 = (short)record[1];
        out->field_08 = (short)record[2];
        out->field_0C = record[3];
        out->field_10 = record[4];
        out->field_14 = record[5];
        out->field_18 = (float)(record[6] >> 10)
                     + (float)(record[6] & 0x3FF) / 1024.0f;
        out->field_1C = (float)(record[7] >> 10)
                     + (float)(record[7] & 0x3FF) / 1024.0f;
        result = 1;
    }
    return result;
}
