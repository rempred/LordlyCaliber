#include "game/combat_pose_record.h"

int func_002052D4(int handle, int directoryIndex)
{
    CombatPoseScratch scratch;
    int low = 1000;
    int high = -1000;
    int index = 0;

    while (func_00205608(handle, directoryIndex, index, &scratch.record)) {
        if (scratch.record.field_04 < low)
            low = scratch.record.field_04;
        if (high < scratch.record.field_04 + scratch.record.field_0C)
            high = scratch.record.field_04 + scratch.record.field_0C;
        index++;
    }
    if (high - low < 0)
        return 0;
    return high - low;
}
