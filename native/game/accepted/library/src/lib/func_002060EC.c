#include "game/combat_pose_pool.h"

u32 *func_002060EC(int handle)
{
    CombatPosePoolRecord *record = D_801D0728;
    int remaining = 19;
    int repeat;

    do {
        if (record->field_00 && record->field_00 == handle)
            return record->field_48;
        record++;
        /* Keep the old count explicit. A postfix loop condition makes KMC
         * compare the decremented count with -1 and reschedule this loop.
         */
        repeat = remaining;
        remaining--;
    } while (repeat);
    return 0;
}
