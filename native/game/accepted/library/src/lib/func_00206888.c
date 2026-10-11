#include "game/combat_pose_pool.h"
#include "game/combat_pose_record.h"

extern void *func_0002E138(u32);
extern void *func_00204A70(void *, u32, u16 *, u8 *, u32);
extern void *func_002043E0(void *, u32, u16 *, u8 *, u32);

#define RESOLVE_SLOT(slot, counter, convert) \
    if (record->slot[index] == 0) { \
        decoded = func_0002E138(record->field_08[originalIndex + 4]); \
        if (*(u16 *)decoded == 0x5554) { \
            record->slot[index] = convert(decoded, \
                selected < decoded[2] ? selected : 0, \
                (u16 *)record->field_6C + (record->field_5C[originalIndex] << 8), \
                &record->field_58[originalIndex], (u8)flags); \
            func_000016C4(decoded); \
        } else if (*(u16 *)decoded == 0x3634) { \
            record->slot[index] = decoded; \
        } \
    } \
    record->counter[index] = 15; \
    return record->slot[index]

void *func_00206888(int handle, CombatPoseIndexedRecord *request,
                    u32 selected, u8 *valueOut)
{
    u32 flags = D_801CEF00;
    CombatPosePoolRecord *record;
    u32 i, index, originalIndex;
    u8 *decoded;

    for (i = 0; i < 20; i++) {
        record = &D_801D0728[i];
        if (record->field_00 != handle || record->field_0C != selected)
            continue;
        index = request->record.field_00;
        originalIndex = index;
        if (request->record.field_14 & 0xC0) {
            flags |= 4;
            if (request->record.field_14 & 0x40)
                index = record->field_1C + request->index;
            else
                index = record->field_1C + request->index + 5;
        }
        if (valueOut)
            *valueOut = record->field_58[originalIndex];
        if (flags & 2) {
            if (flags & 1) {
                RESOLVE_SLOT(field_40, field_68, func_00204A70);
            } else {
                RESOLVE_SLOT(field_38, field_60, func_00204A70);
            }
        } else {
            RESOLVE_SLOT(field_3C, field_64, func_002043E0);
        }
    }
    return 0;
}

#undef RESOLVE_SLOT
