typedef unsigned char u8;
typedef unsigned short u16;

typedef struct Func00195410Record {
    u8 field_00[0x11];
    u8 field_11;
    u8 field_12;
    u8 field_13[0x17];
    u16 field_2A;
    u16 field_2C;
    u16 field_2E;
    u16 field_30;
} Func00195410Record;

typedef struct Func00195410ClassRecord {
    u8 field_00[0x2E];
    u16 field_2E;
    u16 field_30;
    u16 field_32;
    u16 field_34;
    u8 field_36[0x0F];
    u8 field_45;
    u8 field_46[2];
} Func00195410ClassRecord;

extern Func00195410ClassRecord g_func_0019554C_class_records[];
extern int func_00195410_helper(u16 value);

void func_00195410(u16 value, Func00195410Record *record)
{
    int initial_result;
    u8 class_id;
    Func00195410ClassRecord *class_record;

    initial_result = func_00195410_helper(value);
    class_id = record->field_11;
    if (g_func_0019554C_class_records[class_id].field_45 != 0) {
        class_record = &g_func_0019554C_class_records[record->field_12];
    } else {
        class_record = &g_func_0019554C_class_records[class_id];
    }

    if (class_record->field_2E != 0) {
        if ((u8)func_00195410_helper(class_record->field_2E) == (u8)initial_result) {
            record->field_2A = value;
            return;
        }
    }
    if (class_record->field_30 != 0) {
        if ((u8)func_00195410_helper(class_record->field_30) == (u8)initial_result) {
            record->field_2C = value;
            return;
        }
    }
    if (class_record->field_32 != 0) {
        if ((u8)func_00195410_helper(class_record->field_32) == (u8)initial_result) {
            record->field_2E = value;
            return;
        }
    }
    if (class_record->field_34 != 0) {
        if ((u8)func_00195410_helper(class_record->field_34) == (u8)initial_result) {
            record->field_30 = value;
        }
    }
}
