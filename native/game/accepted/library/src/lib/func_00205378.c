#include "game/combat_pose_record.h"

typedef struct CombatPoseBounds {
    int low04;
    int low08;
    int high04;
    int high08;
} CombatPoseBounds;

extern CombatPoseBounds D_801D03A0;

CombatPoseBounds func_00205378(int handle, int directoryIndex)
{
    CombatPoseScratch scratch;
    CombatPoseBounds bounds = D_801D03A0;
    int index = 0;

    while (func_00205608(handle, directoryIndex, index, &scratch.record)) {
        if (scratch.record.field_04 < bounds.low04)
            bounds.low04 = scratch.record.field_04;
        if (scratch.record.field_08 < bounds.low08)
            bounds.low08 = scratch.record.field_08;
        if (bounds.high04 < scratch.record.field_04 + scratch.record.field_0C)
            bounds.high04 = scratch.record.field_04 + scratch.record.field_0C;
        if (bounds.high08 < scratch.record.field_08 + scratch.record.field_10)
            bounds.high08 = scratch.record.field_08 + scratch.record.field_10;
        index++;
    }
    return bounds;
}
