#include "game/combat_pose_pool.h"
#include "game/combat_pose_record.h"

extern int func_002015C8(int, int, int, int);
extern int func_0020161C(int, int, int, int);
extern int func_0020156C(int);
extern int func_000454E0(int);
extern void *func_0002E138(u32);
extern void *func_00204A70(void *, u32, u16 *, u8 *, u32);
extern void *func_002043E0(void *, u32, u16 *, u8 *, u32);

#define RESOLVE_SLOT(slot, counter, convert, argument, tableOffset) \
    if (record->slot[index] == 0) { \
        decoded = func_0002E138(record->field_08[originalIndex + 4]); \
        record->slot[index] = convert(decoded, argument, \
            (u16 *)record->field_6C + (tableOffset), \
            &record->field_58[originalIndex], (u8)flags); \
        func_000016C4(decoded); \
    } \
    record->counter[index] = 15; \
    return record->slot[index]

void *func_00206BE0(int a, int b, CombatPoseIndexedRecord *request,
                    int d, int c, int variant, u8 *valueOut)
{
    CombatPosePoolRecord *record;
    u32 selected = 0;
    u32 previous = 0;
    u8 *decoded;
    u32 flags = D_801CEF00;
    u32 i, index, originalIndex;
    int handle, resolvedVariant;
    u8 kind;

    switch (a) {
    case 0x3B: resolvedVariant = 11; break;
    case 0x3C: resolvedVariant = 13; break;
    case 0x3D: resolvedVariant = 15; break;
    case 0x3E: resolvedVariant = 18; break;
    case 0x39:
    case 0x3F:
    case 0x40: resolvedVariant = 2; break;
    case 0x41: resolvedVariant = 3; break;
    case 0x42: resolvedVariant = 4; break;
    case 0x43: resolvedVariant = 5; break;
    case 0x44: resolvedVariant = 6; break;
    case 0x3A:
    case 0x46: resolvedVariant = 8; break;
    default: resolvedVariant = variant; break;
    }
    variant = resolvedVariant;
    handle = func_002015C8(a, b, c, d);
    for (i = 0; i < 20; i++) {
        record = &D_801D0728[i];
        if (record->field_00 != handle)
            continue;
        index = request->record.field_00;
        kind = record->field_54[index];
        originalIndex = index;
        if (request->record.field_14 & 0xC0) {
            flags |= 4;
            if (request->record.field_14 & 0x40)
                index = record->field_1C + request->index;
            else
                index = record->field_1C + request->index + 5;
        }
        if ((a == 0x39 || a == 0x3F) && kind == 2)
            kind = 1;
        if (kind < 2) {
            switch (kind) {
            case 0:
                selected = func_0020161C(a, b, c, d);
                previous = record->field_0C;
                break;
            case 1:
                selected = (u8)func_0020156C((u16)variant);
                previous = record->field_10;
                break;
            }
            if (previous != selected)
                continue;
            if (valueOut)
                *valueOut = record->field_58[originalIndex];
            if (flags & 2) {
                if (flags & 1) {
                    RESOLVE_SLOT(field_40, field_68, func_00204A70,
                        selected < decoded[2] ? selected : 0,
                        record->field_5C[originalIndex] << 8);
                } else {
                    RESOLVE_SLOT(field_38, field_60, func_00204A70,
                        selected < decoded[2] ? selected : 0,
                        record->field_5C[originalIndex] << 8);
                }
            } else {
                RESOLVE_SLOT(field_3C, field_64, func_002043E0,
                    selected < decoded[2] ? selected : 0,
                    record->field_5C[originalIndex] << 8);
            }
        } else {
            selected = (u8)func_000454E0((u16)variant);
            previous = record->field_14;
            if (previous != selected)
                continue;
            if (valueOut)
                *valueOut = record->field_58[originalIndex];
            if (flags & 2) {
                if (flags & 1) {
                    RESOLVE_SLOT(field_40, field_68, func_00204A70, 0,
                        (record->field_5C[originalIndex] << 8) + (selected << 4));
                } else {
                    RESOLVE_SLOT(field_38, field_60, func_00204A70, 0,
                        (record->field_5C[originalIndex] << 8) + (selected << 4));
                }
            } else {
                RESOLVE_SLOT(field_3C, field_64, func_002043E0, 0,
                    (record->field_5C[originalIndex] << 8) + (selected << 4));
            }
        }
    }
    return 0;
}

#undef RESOLVE_SLOT
