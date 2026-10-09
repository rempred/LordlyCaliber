typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

typedef struct CharacterRecord {
    u8 unk_00[0x11];
    u8 class_11;
    u8 class_12;
    u8 unk_13[0x17];
    u16 equipment_2A;
    u16 equipment_2C;
    u16 equipment_2E;
    u16 equipment_30;
} CharacterRecord;

typedef s32 (*EquipmentSelector)(u8, u8);

extern EquipmentSelector g_func_0018B494_equipment_selectors[];

s32 func_000449ec(CharacterRecord *arg0, s32 arg1)
{
    s32 slot;
    s32 equipment;
    u16 equipment_value;

    slot = arg1 & 0xFF;
    equipment = 0;
    switch (slot) {
    case 0:
        equipment = arg0->equipment_2A;
        break;
    case 1:
        equipment = arg0->equipment_2C;
        break;
    case 2:
        equipment = arg0->equipment_2E;
        break;
    case 3:
        equipment = arg0->equipment_30;
        break;
    }

    equipment_value = equipment & 0xFFFF;
    if (equipment_value == 0) {
        equipment = g_func_0018B494_equipment_selectors[arg1 & 0xFF](
            arg0->class_11, arg0->class_12);
    }
    return equipment & 0xFFFF;
}
