#include "game/scenario_source_record.h"

typedef struct Func001957D0Template {
    u8 field_00;
    u8 field_01;
    u16 field_02;
    s8 field_04[2];
    u8 field_06;
    u8 field_07;
    u8 field_08;
    u16 field_09;
    s8 field_0B[2];
    u8 field_0D[3];
    u8 field_10;
    u8 field_11;
    u16 field_12;
    s8 field_14[2];
    u8 field_16[13];
} __attribute__((packed)) Func001957D0Template;
typedef struct Func001957D0Record52 {
    u8 field_00[0x12];
    u8 field_12;
    u8 field_13[0x21];
} Func001957D0Record52;
typedef struct Func001957D0Record56 {
    u8 field_00[0x12];
    u8 field_12;
    u8 field_13[0x25];
} Func001957D0Record56;
extern u8 g_func_001957D0_active_source_ids[];
extern Func001957D0Record52 g_func_0019554C_records_52[];
extern Func001957D0Record56 g_func_0019554C_records_56[];
extern u16 g_func_001957D0_special_slots[];
extern u16 g_func_001957D0_normal_slots[];
extern u16 g_func_001957D0_special_slot_seed;
extern u16 g_func_001957D0_normal_slot_seed;
extern u8 g_func_001957D0_special_slot_state[];
extern u8 g_func_001957D0_normal_slot_state[];
extern const char g_func_001957D0_record_error[];
extern const char g_func_001957D0_slot_error[];
extern void func_00023780(void *destination, int size);
extern void func_00023940(const char *message);
extern void func_0019554C(
    int record_index,
    Func001957D0Template *source,
    int selector,
    int flag,
    int source_index);
extern u8 func_001957D0_finalize(Func001957D0SourceRecord *record);
void func_001957D0(Func001957D0Template *incoming_source, int source_index)
{
    Func001957D0Template *source;
    u8 special_source_value;
    register u8 source_byte;
    u32 source_record_index;
    u8 source_field_07;
    u8 field_07_member_value;
    int main_record_index;
    int field_07_slot_index;
    int field_07_record_index;
    int field_10_slot_index;
    int scan_index;
    int member_index;
    int source_offset;
    u32 masked_source_index;
    int record_offset;
    int call_record_index;
    int main_pool;
    u8 pool_byte;
    u16 *saved_s7;
    u32 field_10_special_slots;
    u8 source_field_10;
    u16 *slot_cursor;
    Func001957D0SourceRecord *source_record;
    u32 source_work;
    source_byte = (u8)source_index;
    special_source_value = source_byte >> 7;
    special_source_value ^= 1;
    source_byte = source_byte < 30;
    source_byte ^= 1;
    source_byte = -source_byte;
    special_source_value &= source_byte;
    pool_byte = special_source_value;
    source_index &= 0x7F;
    masked_source_index = (s8)source_index;
    source_record_index = (u8)masked_source_index;
    source_record = &g_func_001957D0_source_records[source_record_index];
    g_func_001957D0_active_source_ids[source_record_index] = masked_source_index;
    {
        if (masked_source_index) {
            source_index++;
        } else {
            source_index++;
        }
        source_record->field_00 = source_index;
    }
    source = incoming_source;
    if (pool_byte != 0) {
        source_record->field_01 = 0x81;
    } else {
        source_record->field_01 = 1;
    }
    func_00023780(source_record->field_02, 5);
    main_pool = masked_source_index >= 30;
    main_pool |= pool_byte;
    member_index = 0;
    if (main_pool != 0) {
        scan_index = 1;
        record_offset = 52;
main_records_52:
        main_record_index = scan_index;
        if (((u8 *)g_func_0019554C_records_52)[record_offset + 0x12] == 0) {
            goto main_record_done;
        }
        scan_index++;
        record_offset += 52;
        if (scan_index < 100) {
            goto main_records_52;
        }
        main_record_index = -1;
        goto main_record_done;
    } else {
        scan_index = 1;
        record_offset = 56;
main_records_56:
        main_record_index = scan_index;
        if (((u8 *)g_func_0019554C_records_56)[record_offset + 0x12] == 0) {
            goto main_record_done;
        }
        scan_index++;
        record_offset += 56;
        if (scan_index < 100) {
            goto main_records_56;
        }
        main_record_index = -1;
    }
    {
    u32 helper_source_index;
    int special_pool;
    Func001957D0Template *call_source;
main_record_done:
    if (main_record_index < 0) {
        func_00023940(g_func_001957D0_record_error);
        call_record_index = main_record_index;
        call_source = source;
    } else {
        call_record_index = main_record_index;
        call_source = source;
    }
    helper_source_index = (u8)masked_source_index;
    special_pool = pool_byte;
    func_0019554C(
        call_record_index,
        call_source,
        0,
        special_pool,
        helper_source_index);
    {
        u8 source_field_06;
        u8 *member_entry;
        member_entry = (u8 *)source_record + member_index;
        member_entry[2] = main_record_index;
        source_field_06 = ((u8 *)source)[6];
        member_entry[7] = source_field_06 - 1;
        source_field_07 = ((u8 *)source)[7];
    }
    member_index++;
    if (source_field_07 == 1) {
        register u16 *special_slot_base;
        int pool_compare;
        source_offset = 0;
        pool_compare = helper_source_index < 30;
        pool_compare ^= 1;
        special_pool |= pool_compare;
        special_slot_base = g_func_001957D0_special_slots;
        saved_s7 = g_func_001957D0_normal_slots;
        source_work = (u32)source;
field_07_slot_outer:
        if (((Func001957D0Template *)source_work)->field_0D[0] != 0) {
            scan_index = 0;
            if (special_pool != 0) {
                switch (member_index) {
                case 0:
                    slot_cursor = special_slot_base;
                    break;
                case 1:
                    slot_cursor = special_slot_base;
                    break;
                default:
                    slot_cursor = special_slot_base;
                    break;
                }
field_07_special_slot_scan:
                field_07_slot_index = scan_index;
                if (*slot_cursor == 0) {
                    goto field_07_slot_scan_done;
                }
                scan_index++;
                slot_cursor++;
                if (scan_index < 120) {
                    goto field_07_special_slot_scan;
                }
                field_07_slot_index = -1;
                goto field_07_slot_scan_done;
            } else {
                slot_cursor = saved_s7;
field_07_normal_slot_scan:
                field_07_slot_index = scan_index;
                if (*slot_cursor == 0) {
                    goto field_07_slot_scan_done;
                }
                scan_index++;
                slot_cursor++;
                if (scan_index < 120) {
                    goto field_07_normal_slot_scan;
                }
                field_07_slot_index = -1;
            }
field_07_slot_scan_done:
            if (field_07_slot_index < 0) {
                func_00023940(g_func_001957D0_slot_error);
            }
            if (special_pool != 0) {
                special_slot_base[field_07_slot_index] = g_func_001957D0_special_slot_seed;
                g_func_001957D0_special_slot_state[field_07_slot_index] = 0;
            } else {
                saved_s7[field_07_slot_index] = g_func_001957D0_normal_slot_seed;
                g_func_001957D0_normal_slot_state[field_07_slot_index] = 0;
            }
            {
                u8 *member_entry;
                member_entry = (u8 *)source_record + member_index;
                member_index++;
                member_entry[2] = field_07_slot_index + 100;
                member_entry[7] =
                    ((Func001957D0Template *)source_work)->field_0D[0] - 1;
                source_offset++;
            }
            source_work++;
            if (source_offset < 3) {
                goto field_07_slot_outer;
            }
        }
    } else if (source_field_07 != 0) {
        int pool_compare;
        source_offset = 0;
        saved_s7 = (u16 *)helper_source_index;
        pool_compare = helper_source_index < 30;
        pool_compare ^= 1;
        special_pool |= pool_compare;
        source_work = (u32)source;
field_07_record_outer:
        if (((Func001957D0Template *)source_work)->field_0D[0] != 0) {
            scan_index = 1;
            if (special_pool != 0) {
                record_offset = 52;
field_07_records_52:
                field_07_record_index = scan_index;
                if (((u8 *)g_func_0019554C_records_52)[record_offset + 0x12] == 0) {
                    goto field_07_record_scan_done;
                }
                scan_index++;
                record_offset += 52;
                if (scan_index < 100) {
                    goto field_07_records_52;
                }
                field_07_record_index = -1;
                goto field_07_record_scan_done;
            } else {
                record_offset = 56;
field_07_records_56:
                field_07_record_index = scan_index;
                if (((u8 *)g_func_0019554C_records_56)[record_offset + 0x12] == 0) {
                    goto field_07_record_scan_done;
                }
                scan_index++;
                record_offset += 56;
                if (scan_index < 100) {
                    goto field_07_records_56;
                }
                field_07_record_index = -1;
            }
field_07_record_scan_done:
            {
            int helper_flag;
            if (field_07_record_index < 0) {
                func_00023940(g_func_001957D0_record_error);
                scan_index = field_07_record_index;
                helper_flag = pool_byte;
            } else {
                scan_index = field_07_record_index;
                helper_flag = pool_byte;
            }
            func_0019554C(
                scan_index,
                source,
                1,
                helper_flag,
                (u32)saved_s7);
            }
            {
                slot_cursor =
                    (u16 *)((u8 *)source_record + member_index);
                member_index++;
                ((u8 *)slot_cursor)[2] = field_07_record_index;
                field_07_member_value =
                    ((Func001957D0Template *)source_work)->field_0D[0];
                if (saved_s7) {
                    source_offset++;
                } else {
                    source_offset++;
                }
                ((u8 *)slot_cursor)[7] = field_07_member_value - 1;
            }
            source_work++;
            if (source_offset < 3) {
                goto field_07_record_outer;
            }
        }
    }
    }
    source_field_10 = source->field_10;
    if (source_field_10 == 1) {
        int slot_compare;
        int slot_special;
        u32 normal_slot_base;
        u8 *member_entry;
        slot_compare = masked_source_index < 30;
        slot_compare ^= 1;
        slot_special = pool_byte || slot_compare;
        field_10_special_slots = (u32)g_func_001957D0_special_slots;
        normal_slot_base = (u32)g_func_001957D0_normal_slots;
        source_work = (u32)source;
field_10_slot_outer:
        if (((Func001957D0Template *)source_work)->field_16[0] != 0) {
            scan_index = 0;
            if (slot_special != 0) {
                slot_cursor = (u16 *)field_10_special_slots;
field_10_special_slot_scan:
                field_10_slot_index = scan_index;
                if (*slot_cursor == 0) {
                    goto field_10_slot_scan_done;
                }
                scan_index++;
                slot_cursor++;
                if (scan_index < 120) {
                    goto field_10_special_slot_scan;
                }
                field_10_slot_index = -1;
                goto field_10_slot_scan_done;
            } else {
                slot_cursor = (u16 *)normal_slot_base;
field_10_normal_slot_scan:
                field_10_slot_index = scan_index;
                if (*slot_cursor == 0) {
                    goto field_10_slot_scan_done;
                }
                scan_index++;
                slot_cursor++;
                if (scan_index < 120) {
                    goto field_10_normal_slot_scan;
                }
                field_10_slot_index = -1;
            }
field_10_slot_scan_done:
            if (field_10_slot_index < 0) {
                func_00023940(g_func_001957D0_slot_error);
            }
            if (slot_special != 0) {
                ((u16 *)field_10_special_slots)[field_10_slot_index] = g_func_001957D0_special_slot_seed;
                g_func_001957D0_special_slot_state[field_10_slot_index] = 0;
            } else {
                ((u16 *)normal_slot_base)[field_10_slot_index] = g_func_001957D0_normal_slot_seed;
                g_func_001957D0_normal_slot_state[field_10_slot_index] = 0;
            }
            member_entry = (u8 *)source_record + member_index;
            member_entry[2] = field_10_slot_index + 100;
            member_entry[7] =
                ((Func001957D0Template *)source_work)->field_16[0] - 1;
            source_work++;
            member_index++;
            if ((int)source_work < (int)((u32)source + 3)) {
                goto field_10_slot_outer;
            }
        }
    } else if (source_field_10 != 0) {
        u32 second_source_index;
        int record_compare;
        int record_index;
        int record_special;
        second_source_index = (u8)masked_source_index;
        if (member_index) {
            record_compare = second_source_index < 30;
        } else {
            record_compare = second_source_index < 30;
        }
        record_compare ^= 1;
        source_work = (u32)source;
        record_special = pool_byte | record_compare;
field_10_record_outer:
        if (((Func001957D0Template *)source_work)->field_16[0] != 0) {
            scan_index = 1;
            if (record_special != 0) {
                record_offset = 52;
field_10_records_52:
                if (((u8 *)g_func_0019554C_records_52)[record_offset + 0x12] == 0) {
                    record_index = scan_index;
                    goto field_10_record_scan_done;
                }
                record_index = scan_index++;
                record_offset += 52;
                if (scan_index < 100) {
                    goto field_10_records_52;
                }
                record_index = -1;
                goto field_10_record_scan_done;
            } else {
                record_offset = 56;
field_10_records_56:
                if (((u8 *)g_func_0019554C_records_56)[record_offset + 0x12] == 0) {
                    goto field_10_record_found_56;
                }
                record_index = scan_index++;
                record_offset += 56;
                if (scan_index < 100) {
                    goto field_10_records_56;
                }
                record_index = -1;
                goto field_10_record_scan_done;
field_10_record_found_56:
                record_index = scan_index;
            }
field_10_record_scan_done:
            {
            int helper_flag;
            if (record_index < 0) {
                func_00023940(g_func_001957D0_record_error);
                scan_index = record_index;
                helper_flag = pool_byte;
            } else {
                scan_index = record_index;
                helper_flag = pool_byte;
            }
            func_0019554C(
                scan_index,
                source,
                2,
                helper_flag,
                second_source_index);
            }
            {
                u8 *member_entry;
                member_entry = (u8 *)source_record + member_index;
                member_entry[2] = record_index;
                member_entry[7] =
                    ((Func001957D0Template *)source_work)->field_16[0] - 1;
                source_work++;
            }
            member_index++;
            if ((int)source_work < (int)((u32)source + 3)) {
                goto field_10_record_outer;
            }
        }
    }
    if (pool_byte == 0) {
        source_record->field_18 = func_001957D0_finalize(source_record);
    } else {
        source_record->field_18 = func_001957D0_finalize(source_record);
    }
    func_00023780(source_record->field_0D, 10);
}
