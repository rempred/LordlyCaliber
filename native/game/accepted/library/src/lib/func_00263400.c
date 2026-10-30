#include "common/types.h"

typedef struct SubmissionRecord {
    u16 active;
    u16 flags;
    s32 value04;
    s32 value08;
    s16 code;
    u8 byte0E;
    u8 byte0F;
    u8 byte10;
    u8 byte11;
    u8 byte12;
    u8 pad13;
} SubmissionRecord;

void bzero(void *memory, s32 bytes);
s32 func_00262910(s32 value, s32 argument);
extern u8 D_800E9C12;
extern s32 D_80220DB0;
extern u8 D_80220DB8[];
extern SubmissionRecord *D_80220E08;
extern s32 D_8021FCC0[];
extern u8 D_8021FEE0[];

void func_00263400(s16 code, u8 *arg1, u8 *arg2, u8 *arg3)
{
    s32 slot = D_80220DB0;
    s16 previousCode = 0;
    SubmissionRecord *record = (SubmissionRecord *)D_80220DB8 + slot;
    u8 *flag;
    s32 *entry;
    s32 *table;
    s32 lookupCode;
    s32 result;
    s32 entryValue;
    s32 metadataOffset;
    s32 i;
    SubmissionRecord *walk;

    D_80220DB0 = slot + 1;
    if (D_80220DB0 >= 4) {
        D_80220DB0 = 0;
    }
    if (record->active != 0) {
        for (;;) {
        }
    }
    bzero(record, sizeof(*record));
    flag = &D_800E9C12;
    if (*flag != 1) {
        previousCode = code;
        code = 0x87;
    }
    record->code = code;
    /* KMC needs the volatile byte view to retain the retail reload.
     * The original flag declaration remains unknown. */
    if (*(volatile u8 *)flag != 1) {
        *(u8 **)0x80220F68 = arg1;
        *(u8 **)0x80220F6C = arg2;
        *(s16 *)0x80220F70 = previousCode;
    } else if ((u16)(code - 0x61) < 0x24) {
        *(u8 **)0x80220F68 = arg1;
        *(u8 **)0x80220F6C = arg2;
        *(u8 **)0x80220F70 = arg3;
        *(s16 *)0x80220F74 = code;
    } else {
        *(u8 **)0x80220F68 = arg1;
        *(u8 **)0x80220F6C = arg2;
        *(s16 *)0x80220F70 = code;
        *(s32 *)0x8020A2D0 = code;
    }
    lookupCode = code;
    table = D_8021FCC0;
    entry = &table[lookupCode];
    result = func_00262910(*entry, 0);
    entryValue = *entry;
    record->value04 = result;
    record->value08 = entryValue;
    record->byte0E = arg1[0];
    metadataOffset = lookupCode * 3;
    record->byte0F = D_8021FEE0[metadataOffset];
    record->byte10 = D_8021FEE0[metadataOffset + 1];
    record->byte11 = D_8021FEE0[metadataOffset + 2];
    record->byte12 = 0xFF;
    if ((u16)code < 0x5D || (u16)(code - 0x61) < 0x24 || (u16)(code - 0x85) < 2) {
        record->flags |= 1;
    }
    record->active = 1;
    D_80220E08 = 0;
    i = D_80220DB0 - 1;
    if (i != D_80220DB0) {
        s32 stop = D_80220DB0;
        SubmissionRecord *base = (SubmissionRecord *)D_80220DB8;
        walk = base + i;
        do {
            if (walk->active != 0 && (walk->flags & 1)) {
                D_80220E08 = walk;
                return;
            }
            i--;
            walk--;
            if (i < 0) {
                walk = base + 3;
                i = 3;
            }
        } while (i != stop);
    }
}
