#include "game/combat_pose_pool.h"

u32 func_002061B8(int handle)
{
    CombatPosePoolRecord *record = D_801D0728;
    int remaining = 19;
    int repeat;

    do {
        if (record->field_00 && record->field_00 == handle)
            return record->field_4C;
        record++;
        /* Preserve KMC's old-count test and branch-delay decrement. */
        repeat = remaining;
        remaining--;
    } while (repeat);
    return 0;
}
