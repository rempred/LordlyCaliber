typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;

typedef struct Func0019554CRecord {
    u8 field_00[0x11];
    u8 field_11;
    u8 field_12;
    u8 field_13[0x17];
    u16 field_2A;
    u16 field_2C;
    u16 field_2E;
    u16 field_30;
    u8 field_32;
    u8 field_33;
    u8 field_34;
    u8 field_35;
    u8 field_36;
    u8 field_37;
} Func0019554CRecord;

typedef struct Func0019554CRecord52 {
    u8 bytes[0x34];
} Func0019554CRecord52;

typedef struct Func0019554CClassRecord {
    u8 field_00[0x45];
    u8 field_45;
    u8 field_46[2];
} Func0019554CClassRecord;

typedef struct Func0019554CTemplate {
    u8 field_00;
    u8 field_01;
    u16 field_02;
    s16 field_04;
    u8 field_06;
    u8 field_07;
    u8 field_08;
    u16 field_09;
    s16 field_0B;
    u8 field_0D[3];
    u8 field_10;
    u8 field_11;
    u16 field_12;
    s16 field_14;
    u8 field_16[13];
} __attribute__((packed)) Func0019554CTemplate;

extern Func0019554CRecord g_func_0019554C_records_56[];
extern Func0019554CRecord52 g_func_0019554C_records_52[];
extern Func0019554CClassRecord g_func_0019554C_class_records[];
extern u8 g_func_0019554C_base_level;
extern const char g_func_0019554C_string[];

extern void func_0004501C(Func0019554CRecord *record, u8 level);
extern void func_00195410(u16 value, Func0019554CRecord *record);
extern void func_000FC120(int first, u8 second);
extern int func_0016C454(void);
extern void func_00023780(void *destination, int size);
extern char *func_0002C950(char *destination, const char *source);

void func_0019554C(
    int record_index,
    const Func0019554CTemplate *source,
    int selector,
    u8 flag,
    u8 source_index)
{
    u8 class_id;
    u8 class_value;
    unsigned int bounded_source_index;
    int flag_nonzero;
    s16 combined_value;
    u8 low_value;
    s16 shifted_value;
    Func0019554CRecord *record;
    Func0019554CRecord *call_record;
    Func0019554CRecord *late_record;

    if ((flag != 0) | (source_index >= 30)) {
        record = (Func0019554CRecord *)&g_func_0019554C_records_52[record_index];
    } else {
        record = &g_func_0019554C_records_56[record_index];
    }

    if (selector == 0) {
        class_id = source->field_00;
    } else if (selector == 1) {
        class_id = source->field_07;
    } else {
        class_id = source->field_10;
    }

    record->field_11 = class_id;
    class_value = g_func_0019554C_class_records[class_id].field_45;
    if (class_value == 0) {
        class_value = class_id;
    }
    record->field_12 = class_value;

    if (selector == 0) {
        func_0004501C(record, g_func_0019554C_base_level + source->field_01);
    } else if (selector == 1) {
        func_0004501C(record, g_func_0019554C_base_level + source->field_08);
    } else {
        func_0004501C(record, g_func_0019554C_base_level + source->field_11);
    }

    record->field_2A = 0;
    record->field_2C = 0;
    record->field_2E = 0;
    record->field_30 = 0;

    if (selector == 0) {
        func_00195410(source->field_02, record);
        combined_value = ((const u8 *)source)[4];
        low_value = ((const u8 *)source)[5];
        call_record = record;
    } else if (selector == 1) {
        func_00195410(source->field_09, record);
        combined_value = ((const u8 *)source)[11];
        low_value = ((const u8 *)source)[12];
        call_record = record;
    } else {
        func_00195410(source->field_12, record);
        combined_value = ((const u8 *)source)[20];
        low_value = ((const u8 *)source)[21];
        call_record = record;
    }
    shifted_value = combined_value << 8;
    combined_value = low_value | shifted_value;
    late_record = record;
    if (source->field_14) {
        if (source->field_04) {
            func_00195410(combined_value, call_record);
        } else {
            func_00195410(combined_value, call_record);
        }
    } else {
        func_00195410(combined_value, call_record);
    }

    late_record->field_32 = 0;
    if (selector == 0) {
        late_record->field_33 = 0x82;
    } else {
        late_record->field_33 = 0x80;
    }

    flag_nonzero = flag != 0;
    bounded_source_index = source_index;
    func_000FC120(flag_nonzero | (bounded_source_index >= 30), (u8)record_index);

    if (func_0016C454() != 0) {
        if ((bounded_source_index == 30) & (selector == 0)) {
            func_00023780(late_record, 17);
            func_0002C950((char *)late_record, g_func_0019554C_string);
        }
    }

    if ((flag == 0) & (source_index < 30)) {
        late_record->field_34 = record_index;
        late_record->field_35 = 0;
        late_record->field_36 = 0;
        late_record->field_37 = 0;
    }
}
