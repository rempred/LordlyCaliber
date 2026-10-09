#include "game/combat_pose_record.h"

extern int func_002015C8(int, int, int, int);
extern void func_0020626C(int, int *, int *, int *, int *);
extern u32 *func_002060EC(int);

int func_00205484(int a, int b, int c, int d, int directoryIndex,
                  u32 selected, CombatPoseIndexedRecord *out)
{
    int firstIndex, firstReplacement, secondIndex, secondReplacement;
    int handle = func_002015C8(a, b, c, d);
    u32 *directory;
    u16 *record;
    int result;

    func_0020626C(handle, &firstIndex, &firstReplacement,
                  &secondIndex, &secondReplacement);
    if (directoryIndex == firstIndex) {
        directoryIndex = firstReplacement;
        out->record.field_14 = 0x40;
    } else if (directoryIndex == secondIndex) {
        directoryIndex = secondReplacement;
        out->record.field_14 = 0x80;
    } else {
        out->record.field_14 = 0;
    }

    directory = func_002060EC(handle);
    if (directory != 0)
        record = (u16 *)((u32)directory + directory[directoryIndex]);
    else
        record = 0;

    if (selected >= *record++) {
        result = 0;
    } else {
        record += selected * 8;
        out->index = selected;
        out->record.field_00 = record[0];
        out->record.field_04 = (short)record[1];
        out->record.field_08 = (short)record[2];
        out->record.field_0C = record[3];
        out->record.field_10 = record[4];
        out->record.field_14 |= record[5];
        out->record.field_18 = (float)(record[6] >> 10) +
                              (float)(record[6] & 0x3FF) / 1024.0f;
        out->record.field_1C = (float)(record[7] >> 10) +
                              (float)(record[7] & 0x3FF) / 1024.0f;
        result = 1;
    }
    return result;
}
