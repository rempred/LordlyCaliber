typedef unsigned short u16;

typedef struct EquipmentRecord {
    u16 item;
    u16 field_02;
} EquipmentRecord;

extern EquipmentRecord D_80193AC0[];

int func_00041638(int arg0)
{
    int count;
    EquipmentRecord *record;

    count = 0;
    arg0 &= 0xFFFF;
    record = D_80193AC0;
    for (; count < 0x28; count++, record++) {
        if (record->item == arg0) {
            return count & 0xFFFF;
        }
    }
    return 0x1FF;
}
