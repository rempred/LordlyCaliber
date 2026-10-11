#include "game/combat_pose_pool.h"

void func_0020626C(int handle, int *fieldA8, int *fieldAA,
                  int *fieldAC, int *fieldAE)
{
    CombatPosePoolRecord *record = D_801D0728;
    int remaining = 19;
    int repeat;

    do {
        if (record->field_00 && record->field_00 == handle) {
            *fieldA8 = record->field_A8;
            *fieldAA = record->field_AA;
            *fieldAC = record->field_AC;
            *fieldAE = record->field_AE;
            return;
        }
        record++;
        /* Preserve KMC's old-count test and branch-delay decrement. */
        repeat = remaining;
        remaining--;
    } while (repeat);
}
