#include "game/combat_pose_record.h"

int func_00205230(int handle, int directoryIndex)
{
    CombatPoseScratch scratch;
    int low = 1000;
    int high = -1000;
    int index = 0;

    while (func_00205608(handle, directoryIndex, index, &scratch.record)) {
        if (scratch.record.field_08 < low)
            low = scratch.record.field_08;
        if (high < scratch.record.field_08 + scratch.record.field_10)
            high = scratch.record.field_08 + scratch.record.field_10;
        index++;
    }
    if (high - low < 0)
        return 0;
    return high - low;
}
