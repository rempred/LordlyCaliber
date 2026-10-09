#include "game/combat_types.h"

typedef struct SupplementCopyRecord {
    unsigned char field_00[0x20];
    u16 field_20, field_22, field_24, field_26;
    u16 field_28, field_2A, field_2C, field_2E;
    unsigned char field_30, field_31, field_32, field_33, field_34, field_35;
    u16 field_36, field_38, field_3A, field_3C;
    unsigned char field_3E, field_3F;
    u32 flags, field_44, field_48, field_4C;
    unsigned char field_50[0xA6], field_F6, field_F7;
} SupplementCopyRecord;

extern unsigned char D_80195560[];
extern void func_80093380(void *, u32);

static __inline__ int kind1(SupplementCopyRecord *record)
{
    if (!record) return 0;
    return record->field_4C == 1;
}

void func_0020C908(SupplementCopyRecord *record, int index)
{
    unsigned char *source = D_80195560 + (u32)index * 0x34u;
    u32 value;
    func_80093380(record, 0xF8);
    record->field_48 = source[0x11];
    value = source[0x12];
    record->field_F6 = index;
    record->field_4C = value;
    record->field_31 = source[0x13];
    record->field_33 = source[0x1A];
    record->field_22 = *(u16 *)(source + 0x16);
    record->field_20 = *(u16 *)(source + 0x18);
    record->field_24 = *(u16 *)(source + 0x1C);
    record->field_26 = *(u16 *)(source + 0x1E);
    record->field_28 = *(u16 *)(source + 0x20);
    record->field_2A = *(u16 *)(source + 0x22);
    record->field_2C = *(u16 *)(source + 0x24);
    record->field_2E = *(u16 *)(source + 0x26);
    record->field_30 = source[0x28];
    record->field_34 = record->field_3F = source[0x1B];
    record->field_36 = *(u16 *)(source + 0x2A);
    record->field_38 = *(u16 *)(source + 0x2C);
    record->field_3A = *(u16 *)(source + 0x2E);
    record->field_3C = *(u16 *)(source + 0x30);
    record->field_3E = source[0x32];
    if (kind1(record)) record->field_3E = 0;
    if (source[0x33] & 2) record->flags |= 0x200;
    if (source[0x33] & 4) record->flags |= 2;
    record->flags |= 0x500;
}
