typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef struct CharacterRecord {
    u8 unk_00[0x11];
    u8 class_11;
    u8 class_12;
    u8 level_13;
    u8 unk_14[2];
    u16 stat_16;
    u8 unk_18[4];
    u16 stat_1C;
    u16 stat_1E;
    u16 stat_20;
    u16 stat_22;
    u16 stat_24;
    u16 stat_26;
    u8 luck_28;
    u8 unk_29;
    u16 equipment_2A;
    u16 equipment_2C;
    u16 equipment_2E;
    u16 equipment_30;
} CharacterRecord;

typedef struct ClassRecord {
    u8 unk_00[0x0A];
    u8 growth_0A;
    u8 unk_0B[3];
    u8 growth_0E;
    u8 unk_0F[3];
    u8 growth_12;
    u8 unk_13[3];
    u8 growth_16;
    u8 unk_17[3];
    u8 growth_1A;
    u8 unk_1B[3];
    u8 growth_1E;
    u8 unk_1F[3];
    u8 growth_22;
    u8 unk_23[0x25];
} ClassRecord;

typedef s32 (*EquipmentSelector)(u8, u8);

extern ClassRecord g_func_0019554C_class_records[];
extern EquipmentSelector g_func_0018B494_equipment_selectors[];
extern s32 rand(void);
extern s8 func_000457e4(s32);
extern s8 func_00045820(s32);
extern s8 func_00045840(s32);
extern s8 func_0004585c(s32);
extern s8 func_00045878(s32);
extern s8 func_00045898(s32);
extern s8 func_000458b8(s32);

void func_00044aa4(CharacterRecord *arg0)
{
    s16 luck_delta;
    s32 class_index;
    u8 class_value;
    s32 random_value;
    s32 second_random;
    s32 modulo_temp;
    s32 second_random_1;
    s32 modulo_temp_1;
    s32 second_random_2;
    s32 modulo_temp_2;
    s32 second_random_3;
    s32 modulo_temp_3;
    s32 second_random_4;
    s32 modulo_temp_4;
    s32 second_random_5;
    s32 modulo_temp_5;
    s32 second_random_6;
    s32 modulo_temp_6;
    s32 growth_12;
    s32 growth_16;
    s32 growth_1A;
    s32 growth_1E;
    s32 growth_22;
    u8 equipment_index;
    s32 loop_index;
    s32 equipment_id;
    u16 growth_0A;
    u16 growth_0E;
    u8 growth_22_base;
    s32 equipment;
    u16 current;
    u16 equipment_value;
    u16 value;
    u16 result_16;
    u16 result_1C;
    u16 result_1E;
    u16 result_20;
    u16 result_22;
    u16 result_24;
    u16 result_26;
    u16 luck_result;
    s32 level;
    s32 class_id;
    ClassRecord *class_record;

    class_id = *((u8 *)arg0 + 0x12);
    level = *((u8 *)arg0 + 0x13);
    if (level < 99U) {
        *((u8 *)arg0 + 0x13) = level + 1;
        class_value = class_id;
        class_index = class_value & 0xFF;
        class_record = &g_func_0019554C_class_records[class_index];

        random_value = class_index == 1
            ? 1
            : (random_value = rand(),
               second_random = rand(),
               modulo_temp = (s32)((u32)random_value >> 31),
               modulo_temp = (random_value + modulo_temp) >> 1,
               modulo_temp = modulo_temp << 1,
               random_value = random_value - modulo_temp,
               modulo_temp = (s32)((u32)second_random >> 31),
               modulo_temp = (second_random + modulo_temp) >> 1,
               modulo_temp = modulo_temp << 1,
               second_random = second_random - modulo_temp,
               random_value + second_random);
        growth_0A = class_record->growth_0A + random_value;

        random_value = (u8)class_value == 1
            ? 1
            : (random_value = rand(),
               second_random_1 = rand(),
               modulo_temp_1 = (s32)((u32)random_value >> 31),
               modulo_temp_1 = (random_value + modulo_temp_1) >> 1,
               modulo_temp_1 = modulo_temp_1 << 1,
               random_value = random_value - modulo_temp_1,
               modulo_temp_1 = (s32)((u32)second_random_1 >> 31),
               modulo_temp_1 = (second_random_1 + modulo_temp_1) >> 1,
               modulo_temp_1 = modulo_temp_1 << 1,
               second_random_1 = second_random_1 - modulo_temp_1,
               random_value + second_random_1);
        growth_0E = class_record->growth_0E + random_value;

        random_value = (u8)class_value == 1
            ? 1
            : (random_value = rand(),
               second_random_2 = rand(),
               modulo_temp_2 = (s32)((u32)random_value >> 31),
               modulo_temp_2 = (random_value + modulo_temp_2) >> 1,
               modulo_temp_2 = modulo_temp_2 << 1,
               random_value = random_value - modulo_temp_2,
               modulo_temp_2 = (s32)((u32)second_random_2 >> 31),
               modulo_temp_2 = (second_random_2 + modulo_temp_2) >> 1,
               modulo_temp_2 = modulo_temp_2 << 1,
               second_random_2 = second_random_2 - modulo_temp_2,
               random_value + second_random_2);
        growth_12 = class_record->growth_12 + random_value;

        random_value = (u8)class_value == 1
            ? 1
            : (random_value = rand(),
               second_random_3 = rand(),
               modulo_temp_3 = (s32)((u32)random_value >> 31),
               modulo_temp_3 = (random_value + modulo_temp_3) >> 1,
               modulo_temp_3 = modulo_temp_3 << 1,
               random_value = random_value - modulo_temp_3,
               modulo_temp_3 = (s32)((u32)second_random_3 >> 31),
               modulo_temp_3 = (second_random_3 + modulo_temp_3) >> 1,
               modulo_temp_3 = modulo_temp_3 << 1,
               second_random_3 = second_random_3 - modulo_temp_3,
               random_value + second_random_3);
        growth_16 = class_record->growth_16 + random_value;

        random_value = (u8)class_value == 1
            ? 1
            : (random_value = rand(),
               second_random_4 = rand(),
               modulo_temp_4 = (s32)((u32)random_value >> 31),
               modulo_temp_4 = (random_value + modulo_temp_4) >> 1,
               modulo_temp_4 = modulo_temp_4 << 1,
               random_value = random_value - modulo_temp_4,
               modulo_temp_4 = (s32)((u32)second_random_4 >> 31),
               modulo_temp_4 = (second_random_4 + modulo_temp_4) >> 1,
               modulo_temp_4 = modulo_temp_4 << 1,
               second_random_4 = second_random_4 - modulo_temp_4,
               random_value + second_random_4);
        growth_1A = class_record->growth_1A + random_value;

        random_value = (u8)class_value == 1
            ? 1
            : (random_value = rand(),
               second_random_5 = rand(),
               modulo_temp_5 = (s32)((u32)random_value >> 31),
               modulo_temp_5 = (random_value + modulo_temp_5) >> 1,
               modulo_temp_5 = modulo_temp_5 << 1,
               random_value = random_value - modulo_temp_5,
               modulo_temp_5 = (s32)((u32)second_random_5 >> 31),
               modulo_temp_5 = (second_random_5 + modulo_temp_5) >> 1,
               modulo_temp_5 = modulo_temp_5 << 1,
               second_random_5 = second_random_5 - modulo_temp_5,
               random_value + second_random_5);
        growth_1E = class_record->growth_1E + random_value;

        random_value = (u8)class_value == 1
            ? 1
            : (random_value = rand(),
               second_random_6 = rand(),
               modulo_temp_6 = (s32)((u32)random_value >> 31),
               modulo_temp_6 = (random_value + modulo_temp_6) >> 1,
               modulo_temp_6 = modulo_temp_6 << 1,
               random_value = random_value - modulo_temp_6,
               modulo_temp_6 = (s32)((u32)second_random_6 >> 31),
               modulo_temp_6 = (second_random_6 + modulo_temp_6) >> 1,
               modulo_temp_6 = modulo_temp_6 << 1,
               second_random_6 = second_random_6 - modulo_temp_6,
               random_value + second_random_6);
        growth_22_base = class_record->growth_22;
        luck_delta = 0;
        loop_index = 0;
        growth_22 = growth_22_base + random_value;

        do {
            s32 slot;

            equipment_index = loop_index;
            slot = equipment_index & 0xFF;
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
                equipment = g_func_0018B494_equipment_selectors[equipment_index & 0xFF](
                    arg0->class_11, arg0->class_12);
            }
            equipment_id = equipment & 0xFFFF;
            if (equipment_id == 0) {
                goto equipment_advance;
            }
            growth_0A += func_000457e4(equipment_id);
            growth_0E += func_000457e4(equipment_id);
            growth_16 += func_00045820(equipment_id);
            growth_1E += func_00045840(equipment_id);
            growth_22 += func_0004585c(equipment_id);
            growth_12 += func_00045878(equipment_id);
            growth_1A += func_00045898(equipment_id);
            luck_delta += func_000458b8(equipment_id);

equipment_advance:
            loop_index += 1;
        } while (loop_index < 4);

        current = arg0->stat_16;
        result_16 = (current + growth_0A) < 1000;
        if (result_16 != 0) {
            result_16 = current + growth_0A;
        } else {
            result_16 = 999;
        }
        arg0->stat_16 = result_16;

        current = arg0->stat_1C;
        result_1C = (current + growth_0E) < 1000;
        if (result_1C != 0) {
            result_1C = current + growth_0E;
        } else {
            result_1C = 999;
        }
        arg0->stat_1C = result_1C;

        current = arg0->stat_1E;
        result_1E = (current + (growth_12 & 0xFFFF)) < 1000;
        if (result_1E != 0) {
            result_1E = current + growth_12;
        } else {
            result_1E = 999;
        }
        arg0->stat_1E = result_1E;

        current = arg0->stat_20;
        result_20 = (current + (growth_16 & 0xFFFF)) < 1000;
        if (result_20 != 0) {
            result_20 = current + growth_16;
        } else {
            result_20 = 999;
        }
        arg0->stat_20 = result_20;

        current = arg0->stat_22;
        result_22 = (current + (growth_1A & 0xFFFF)) < 1000;
        if (result_22 != 0) {
            result_22 = current + growth_1A;
        } else {
            result_22 = 999;
        }
        arg0->stat_22 = result_22;

        current = arg0->stat_24;
        result_24 = (current + (growth_1E & 0xFFFF)) < 1000;
        if (result_24 != 0) {
            result_24 = current + growth_1E;
        } else {
            result_24 = 999;
        }
        arg0->stat_24 = result_24;

        current = arg0->stat_26;
        result_26 = (current + (growth_22 & 0xFFFF)) < 1000;
        if (result_26 != 0) {
            result_26 = current + growth_22;
        } else {
            result_26 = 999;
        }
        arg0->stat_26 = result_26;

        if ((arg0->luck_28 + luck_delta) < 0) {
            arg0->luck_28 = 0;
            return;
        }
        luck_result = (arg0->luck_28 + luck_delta) < 101;
        if (luck_result != 0) {
            luck_result = arg0->luck_28 + luck_delta;
        } else {
            luck_result = 100;
        }
        arg0->luck_28 = luck_result;
    }
}
