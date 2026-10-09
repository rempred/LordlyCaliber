#include "game/combat_pose_pool.h"

void func_002062D0(int handle, u8 *out)
{
    CombatPosePoolRecord *record = D_801D0728;
    int remaining = 19;
    int repeat;

    do {
        if (record->field_00 && record->field_00 == handle) {
            out[0] = record->field_A2[0];
            out[1] = record->field_A2[1];
            out[2] = record->field_A2[2];
            out[3] = record->field_A2[3];
            out[4] = record->field_A2[4];
            out[5] = record->field_A2[5];
            return;
        }
        record++;
        /* Preserve KMC's old-count test and branch-delay decrement. */
        repeat = remaining;
        remaining--;
    } while (repeat);
}
