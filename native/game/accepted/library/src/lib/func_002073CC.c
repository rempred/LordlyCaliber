#include "game/combat_pose_pool.h"

extern u32 func_8009DAF4(u32);
extern void *func_80071C04(u32);
extern void func_8009DBB8(void *, u32);
extern u32 *func_0002E138(u32);
extern u32 *func_0002E348(u32);
extern void func_00023460(const void *, void *, u32);

void func_002073CC(int key, int value)
{
    CombatPosePoolRecord *scan = D_801D0728;
    CombatPosePoolRecord *record;
    u32 i, old;
    u32 *offsets;
    u32 *decoded;
    u32 count, byteBytes, wordBytes;
    /* Separate publication temporaries preserve KMC's argument scheduling. */
    u8 *cursor, *p54, *p58, *p5C, *p60, *p3C, *p64, *p40, *p68;

    for (i = 0; i < 20; i++, scan++) {
        if (scan->field_00 == key && scan->field_0C == value)
            return;
    }
    record = D_801D0728;
    /* Keep the old counter across the pointer advance. The explicit exit
     * prevents KMC from rotating and duplicating the first field test.
     */
    i = 19;
    do {
        if (record->field_00 == 0)
            goto initialize;
        old = i;
        record++;
        i--;
    } while (old != 0);
initialize:

    record->field_00 = key;
    offsets = func_80071C04(func_8009DAF4(0x003B6CD0));
    func_8009DBB8(offsets, 0x003B6CD0);
    record->field_04 = offsets[key - 1];
    record->field_08 = func_00001330(func_8009DAF4(record->field_04));
    func_8009DBB8(record->field_08, record->field_04);
    func_000016C4(offsets);
    record->field_48 = func_0002E138(record->field_08[0]);
    record->field_44 = func_0002E138(record->field_08[1]);
    decoded = func_0002E348(record->field_08[2]);
    record->field_6C = func_0002E138(record->field_08[3]);
    record->field_50 = record->field_48[0] >> 2;
    record->field_4C = record->field_44[0] >> 2;
    record->field_0C = value;
    record->field_10 = value;
    record->field_14 = value;
    record->field_18 = decoded[0];
    record->field_20 = decoded[1];
    count = record->field_18;
    record->field_24 = decoded[2];
    record->field_28 = decoded[3];
    byteBytes = (count + 7) & ~7;
    wordBytes = (count * 4 + 7) & ~7;
    cursor = func_00001330(wordBytes + byteBytes + byteBytes + byteBytes +
                          byteBytes + wordBytes + byteBytes + wordBytes + byteBytes);
    record->field_38 = (void **)cursor;
    record->field_2C = cursor;
    p54 = cursor + wordBytes;
    record->field_54 = p54;
    p58 = p54 + byteBytes;
    record->field_58 = p58;
    p5C = p58 + byteBytes;
    record->field_5C = p5C;
    p60 = p5C + byteBytes;
    record->field_60 = p60;
    p3C = p60 + byteBytes;
    record->field_3C = (void **)p3C;
    p64 = p3C + wordBytes;
    record->field_64 = p64;
    p40 = p64 + byteBytes;
    record->field_40 = (void **)p40;
    p68 = p40 + wordBytes;
    record->field_68 = p68;
    func_00023780(record->field_38, wordBytes);
    func_00023780(record->field_3C, wordBytes);
    func_00023780(record->field_40, wordBytes);
    func_00023780(record->field_58, byteBytes);
    func_00023780(record->field_60, byteBytes);
    func_00023780(record->field_64, byteBytes);
    func_00023780(record->field_68, byteBytes);
    func_00023460(decoded + 8, record->field_54, record->field_18);
    func_00023460((u8 *)decoded + decoded[7], record->field_5C, record->field_18);
    func_000016C4(decoded);
}
