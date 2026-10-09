#ifndef COMBAT_POSE_RECORD_H
#define COMBAT_POSE_RECORD_H

#include "combat_types.h"

/* Common decoded prefix written by func_00205484 and func_00205608. */
typedef struct CombatPoseRecord {
    int field_00;
    int field_04;
    int field_08;
    int field_0C;
    int field_10;
    int field_14;
    float field_18;
    float field_1C;
} CombatPoseRecord;

typedef struct CombatPoseIndexedRecord {
    CombatPoseRecord record;
    unsigned char index;
} CombatPoseIndexedRecord;

/* Preserve the bounds callers' 0x28-byte local buffer. Only the decoded
 * prefix is accessed; the extra storage has no established field meaning.
 */
typedef union CombatPoseScratch {
    CombatPoseRecord record;
    int storage[10];
} CombatPoseScratch;

extern int func_00205608(int handle, int directoryIndex, u32 selected,
                       CombatPoseRecord *out);

#endif
