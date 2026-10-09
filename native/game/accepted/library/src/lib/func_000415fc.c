typedef unsigned short u16;

typedef struct EquipmentRecord {
    u16 item;
    u16 field_02;
} EquipmentRecord;

extern EquipmentRecord D_80196B00[];

int func_000415fc(int arg0)
{
    int count;
    EquipmentRecord *record;

    count = 0;
    arg0 &= 0xFFFF;
    record = D_80196B00;
    for (; count < 0x116; count++, record++) {
        if (record->item == arg0) {
            return count & 0xFFFF;
        }
    }
    return 0x1FF;
}
