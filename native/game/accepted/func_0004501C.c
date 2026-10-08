typedef unsigned char u8;
typedef unsigned short u16;

typedef struct Func0004501CRecord {
    u8 field_00[0x11];
    u8 field_11;
    u8 field_12;
    u8 field_13;
    u8 field_14;
    u8 field_15;
    u16 field_16;
    u16 field_18;
    u8 field_1A;
    u8 field_1B;
    u16 field_1C;
    u16 field_1E;
    u16 field_20;
    u16 field_22;
    u16 field_24;
    u16 field_26;
    u8 field_28;
} Func0004501CRecord;

typedef struct Func0004501CClassRecord {
    u8 field_00[0x05];
    u8 field_05;
    u8 field_06[0x02];
    u16 field_08;
    u8 field_0A[0x02];
    u16 field_0C;
    u8 field_0E[0x02];
    u16 field_10;
    u8 field_12[0x02];
    u16 field_14;
    u8 field_16[0x02];
    u16 field_18;
    u8 field_1A[0x02];
    u16 field_1C;
    u8 field_1E[0x02];
    u16 field_20;
    u8 field_22;
    u8 field_23;
    u8 field_24;
    u8 field_25[0x1C];
    u8 field_41;
    u8 field_42;
    u8 field_43;
    u8 field_44;
    u8 field_45;
    u8 field_46;
    u8 field_47;
} Func0004501CClassRecord;

extern Func0004501CClassRecord g_func_0019554C_class_records[];
extern int func_0002CBCC(void);
extern void func_00044aa4(Func0004501CRecord *record);

static __inline__ u8 func_0004501C_select_loop(u8 original_class, u8 alternate_class, int original_offset, int alternate_value, int current_level, int invalid_class)
{
    u8 class_index;
    u8 selector_level;

    selector_level = current_level;
    if (*((u8 *)g_func_0019554C_class_records + original_offset + 0x45) != alternate_value) {
        class_index = alternate_class;
    } else {
        class_index = original_class;
    }

    if (g_func_0019554C_class_records[class_index].field_42 != (invalid_class + invalid_class - invalid_class) &&
        (u8)selector_level < g_func_0019554C_class_records[class_index].field_42) {
        class_index = g_func_0019554C_class_records[class_index].field_41;
        goto transition_done;
    }
    if (g_func_0019554C_class_records[class_index].field_44 != invalid_class &&
        (u8)selector_level < g_func_0019554C_class_records[class_index].field_44) {
        class_index = g_func_0019554C_class_records[class_index].field_43;
    }
transition_done:
    return class_index;
}

void func_0004501C(Func0004501CRecord *record, int level)
{
    u8 original_class;
    u8 alternate_class;
    u8 class_index;
    u8 selected_class;
    u8 class_value;
    u8 initial_class_value;
    int field_1B_value;
    u8 loop_class;
    int counter;
    int loop_counter;
    int current_level;
    int original_offset;
    int alternate_value;
    int invalid_class;
    int initial_scaled_raw;
    int initial_offset_value;
    Func0004501CClassRecord *class_record;
    Func0004501CRecord *record_alias;

    original_class = record->field_11;
    alternate_class = record->field_12;

    if (g_func_0019554C_class_records[original_class].field_45 != alternate_class) {
        class_index = alternate_class;
    } else {
        class_index = original_class;
    }

    if (g_func_0019554C_class_records[class_index].field_42 != 0xFF) {
        int first_minimum_level;

        first_minimum_level = 1;
        if ((u8)first_minimum_level < g_func_0019554C_class_records[class_index].field_42) {
            class_index = g_func_0019554C_class_records[class_index].field_41;
            goto initial_transition_done;
        }
    }
    if (g_func_0019554C_class_records[class_index].field_44 != 0xFF) {
        int second_minimum_level;

        second_minimum_level = 1;
        if ((u8)second_minimum_level < g_func_0019554C_class_records[class_index].field_44) {
            class_index = g_func_0019554C_class_records[class_index].field_43;
            goto initial_transition_done;
        }
    }
initial_transition_done:
    initial_scaled_raw = (class_index << 3) + class_index;
    initial_offset_value = initial_scaled_raw << 3;
    counter = class_index;
    do {
        initial_scaled_raw = *((u8 *)g_func_0019554C_class_records + initial_offset_value + 5);
        initial_offset_value = (u8)initial_scaled_raw;
    } while (0);

    if (initial_offset_value == 3) {
        record->field_14 = 0;
    } else if (initial_offset_value == 4) {
        record->field_14 = func_0002CBCC() & 1;
    } else {
        record->field_14 = initial_scaled_raw;
    }

    class_record = &g_func_0019554C_class_records[counter];
    record->field_16 = class_record->field_08;
    record->field_1C = class_record->field_0C;
    record->field_1E = class_record->field_10;
    record->field_20 = class_record->field_14;
    record->field_22 = class_record->field_18;
    record->field_24 = class_record->field_1C;
    record->field_26 = class_record->field_20;

    if (g_func_0019554C_class_records[original_class].field_45 == alternate_class) {
        class_value = g_func_0019554C_class_records[original_class].field_46;
    } else {
        class_value = g_func_0019554C_class_records[alternate_class].field_46;
    }
    record_alias = record;
    if (class_value == 0xFF) {
        record_alias->field_1A = (func_0002CBCC() % 4) + 1;
    } else {
        record_alias->field_1A = class_value;
    }

    if (g_func_0019554C_class_records[original_class].field_45 == alternate_class) {
        initial_class_value = g_func_0019554C_class_records[original_class].field_24;
        field_1B_value = initial_class_value;
        counter = 1;
    } else {
        initial_class_value = g_func_0019554C_class_records[alternate_class].field_24;
        field_1B_value = initial_class_value;
        counter = 1;
    }
    loop_counter = counter;
    record_alias->field_1B = field_1B_value;
    record_alias->field_13 = 1;

    if (loop_counter >= (u8)level) {
        goto loop_done;
    }
    original_offset = original_class * 0x48;
    alternate_value = (u8)alternate_class;
    invalid_class = 0xFF;
loop_body:
    current_level = loop_counter;
    loop_class = func_0004501C_select_loop(original_class, alternate_class, original_offset, alternate_value, current_level, invalid_class);

    record_alias->field_12 = loop_class;
    record_alias->field_11 = loop_class;
    func_00044aa4(record_alias);
    loop_counter++;
    if (loop_counter < (u8)level) {
        goto loop_body;
    }
loop_done:
    ;

    record_alias->field_18 = record_alias->field_16;
    record_alias->field_11 = original_class;
    record_alias->field_12 = alternate_class;

    if (g_func_0019554C_class_records[original_class].field_45 == alternate_class) {
        record_alias->field_28 = g_func_0019554C_class_records[original_class].field_23;
    } else {
        record_alias->field_28 = g_func_0019554C_class_records[alternate_class].field_23;
    }
}
