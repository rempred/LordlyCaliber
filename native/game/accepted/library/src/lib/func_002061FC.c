#include "game/combat_pose_pool.h"

void func_002061FC(int handle, u32 *replacement)
{
    CombatPosePoolRecord *record = D_801D0728;
    int remaining = 19;
    int repeat;

    do {
        if (record->field_00 && record->field_00 == handle) {
            func_000016C4(record->field_44);
            record->field_44 = replacement;
            return;
        }
        record++;
        /* Preserve KMC's old-count test and branch-delay decrement. */
        repeat = remaining;
        remaining--;
    } while (repeat);
}
