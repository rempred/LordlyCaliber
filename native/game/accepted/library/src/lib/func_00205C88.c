#include "game/combat_pose_pool.h"

extern int func_002015C8(int, int, int, int);
extern int func_0020161C(int, int, int, int);
extern int func_0020156C(int);
extern int func_000454E0(int);
extern u32 func_8009DAF4(u32);
extern void *func_80071C04(u32);
extern void func_8009DBB8(void *, u32);
extern u32 *func_0002E138(u32);
extern u32 *func_0002E348(u32);
extern void func_00023460(const void *, void *, u32);

#define RECORD_BYTES(token) (*(u8 *)(0x801CEEE0 + (token)))

CombatPosePoolRecord *func_00205C88(CombatPosePoolRecord *record,
                                 int a, int b, int c, int d, int variant)
{
    u32 *offsets;
    u32 *decoded;
    /* Keep this allocation count separate from the parser count below:
     * sharing the local changes KMC's saved-register allocation.
     */
    u32 entryCount, wordBytes, byteBytes, totalBytes;
    u32 position;
    u32 *directory;
    u8 *cursor;
    u32 index, count;

    switch (a) {
    case 0x3B: variant = 11; break;
    case 0x3C: variant = 13; break;
    case 0x3D: variant = 15; break;
    case 0x3E: variant = 18; break;
    case 0x39:
    case 0x3F:
    case 0x40: variant = 2; break;
    case 0x41: variant = 3; break;
    case 0x42: variant = 4; break;
    case 0x43: variant = 5; break;
    case 0x44: variant = 6; break;
    case 0x3A:
    case 0x46: variant = 8; break;
    }

    record->field_00 = func_002015C8(a, b, c, d);
    if (record->field_00 == 0)
        return 0;
    offsets = func_80071C04(func_8009DAF4(0x003B6CD0));
    func_8009DBB8(offsets, 0x003B6CD0);
    record->field_04 = offsets[record->field_00 - 1];
    record->field_08 = func_00001330(func_8009DAF4(record->field_04));
    if (record->field_08 == 0) {
        func_000016C4(offsets);
        return 0;
    }
    func_8009DBB8(record->field_08, record->field_04);
    func_000016C4(offsets);
    record->field_48 = func_0002E138(record->field_08[0]);
    record->field_44 = func_0002E138(record->field_08[1]);
    decoded = func_0002E348(record->field_08[2]);
    record->field_6C = func_0002E138(record->field_08[3]);
    record->field_50 = record->field_48[0] >> 2;
    record->field_4C = record->field_44[0] >> 2;
    record->field_0C = func_0020161C(a, b, c, d);
    record->field_10 = (u8)func_0020156C((u16)variant);
    record->field_14 = (u8)func_000454E0((u16)variant);
    record->field_1C = decoded[0];
    record->field_18 = decoded[0] + 10;
    record->field_20 = decoded[1];
    entryCount = record->field_18;
    record->field_24 = decoded[2];
    record->field_28 = decoded[3];
    record->field_A2[0] = ((u8 *)decoded)[0x10];
    record->field_A2[1] = ((u8 *)decoded)[0x11];
    record->field_A2[2] = ((u8 *)decoded)[0x12];
    record->field_A2[3] = ((u8 *)decoded)[0x13];
    record->field_A2[4] = ((u8 *)decoded)[0x14];
    record->field_A2[5] = ((u8 *)decoded)[0x15];
    byteBytes = (entryCount + 7) & ~7;
    wordBytes = (entryCount * 4 + 7) & ~7;
    totalBytes = wordBytes + byteBytes + byteBytes + byteBytes + byteBytes +
                 wordBytes + byteBytes + wordBytes + byteBytes;
    record->field_2C = func_00001330(totalBytes);
    record->field_30 = record->field_2C + totalBytes;
    position = (u32)record->field_2C;
    record->field_38 = (void **)position;
    position += wordBytes;
    record->field_54 = (u8 *)position;
    position += byteBytes;
    record->field_58 = (u8 *)position;
    position += byteBytes;
    record->field_5C = (u8 *)position;
    position += byteBytes;
    record->field_60 = (u8 *)position;
    position += byteBytes;
    record->field_3C = (void **)position;
    position += wordBytes;
    record->field_64 = (u8 *)position;
    position += byteBytes;
    record->field_40 = (void **)position;
    position += wordBytes;
    record->field_68 = (u8 *)position;
    func_00023780(record->field_38, wordBytes);
    func_00023780(record->field_3C, wordBytes);
    func_00023780(record->field_40, wordBytes);
    func_00023780(record->field_58, byteBytes);
    func_00023780(record->field_60, byteBytes);
    func_00023780(record->field_64, byteBytes);
    func_00023780(record->field_68, byteBytes);
    func_00023460((u8 *)decoded + 0x20, record->field_54, record->field_1C);
    func_00023460((u8 *)decoded + decoded[7], record->field_5C, record->field_1C);

    directory = record->field_44;
    cursor = (u8 *)directory + directory[0x98 / 4];
    count = *cursor++;
    record->field_AC = 0xFFFF;
    record->field_AE = 0xFFFF;
    record->field_A8 = 0xFFFF;
    record->field_AA = 0xFFFF;
    for (index = 0; index < count; index++) {
        if (*cursor == 1) {
            record->field_AA = record->field_A8;
            record->field_A8 = cursor[1];
        }
        cursor += RECORD_BYTES(*cursor);
    }
    if (record->field_4C >= 0x58) {
        cursor = (u8 *)directory + directory[0x160 / 4];
        count = *cursor++;
        for (index = 0; index < count; index++) {
            if (*cursor == 1) {
                record->field_AE = record->field_AC;
                record->field_AC = cursor[1];
            }
            cursor += RECORD_BYTES(*cursor);
        }
    }
    func_000016C4(decoded);
    return record;
}
