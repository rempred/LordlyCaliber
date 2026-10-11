typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

s32 func_00045480(u16);
s32 func_000454e0(u16);
s32 func_00201584(s32, s32);
s32 func_00201798(s32, s32, s32, s32);
s32 func_0020BFE4(void);
s32 func_0020BFF8(void *);
s32 func_0020C014(void *);
s32 func_0020C104(void *);
s32 func_0020C2C0(void *);
s32 func_0020C32C(void *);
u16 func_0020C448(void *);
void *func_0020C478(unsigned int index);
s32 func_0020D72C(void *);
void func_0021D200(s32, s32, void *);
void func_0021D230(s32, s32, void *, s32);
void func_0021D25C(s32, s32, void *, s32, s32);
void func_0021D28C(s32, s32, void *, s32, s32, s32);
void func_0021D2C0(s32, s32, s32, s32, s32, s32, s32);
s32 func_0022257C(void *, s32, s32 *);
s32 func_0022A4E0(void *);
void func_0022ADFC(void *, s32, s32, s32, s32, u8, s32, s32);
s32 func_00233210(void *, void *);
s32 func_00233F38(void *);
s32 func_0023431C(void *, s32, s32);
s32 func_002361EC(void *);
s32 func_002363FC(void *);
u32 func_002365BC(void *, void *, s32, s32);
s32 func_00237750(void *, void *, s32, s32, s32);
s32 func_00237810(void *, void *);
s32 func_00237890(void *, void *);
s32 func_00237AF8(void *, s32);
s32 memset_00023780(s32 *, s32);
s32 rand(void);

s32 func_0022BFF8(void *record, s32 *out_time) {
    s32 buffer[6];
    s32 *time_out;
    s32 first_time;
    s32 end_time;
    s32 duration;
    u16 selector_id;
    s32 secondary_result;
    s32 tail_end, tail_pre, tail_first, tail_duration, tail_last, result;
    u8 *bridge_data;
    s32 action_byte;
    u32 loaded;
    u32 early_loaded;
    s32 event_selector;
    s32 early_input_time;
    s32 target_time3;
    s32 source_value;
    u32 sample0;
    s32 source_code0;
    s32 source_code1;
    s32 source_code2;
    u32 sample1;
    s32 value;
    s32 stream_code;
    s32 target58_0;
    s32 target_index;
    s32 increment;
    s32 target58_1;
    s32 record_time1;
    s32 record58_0;
    s32 target_time2;
    s32 record58_1;
    s32 record_time0;
    s32 target_time0;
    s32 secondary_selector;
    s32 bridge_selector;
    s32 offset;
    s32 distance;
    s32 time;
    s32 field_code;
    s32 aux_value;
    s32 record_time2;
    s32 code_byte;
    s32 early_record_time;
    s32 adjust;
    u32 check_value;
    register u32 latest_time;
    register u32 early_latest_time;
    void *target;
    aux_value = 0;
    time_out = out_time;
    secondary_result = 0;
    memset_00023780(buffer, 0x18);
    target_index = func_00233F38(record);
    *(s32 *)((u8 *)record + 0x68) = target_index;
    if (target_index < 0) {
        early_record_time = *(s32 *)((u8 *)record + 0x94);
        early_input_time = *time_out;
        if (early_record_time < early_input_time) {
            early_record_time = early_input_time;
        }
        early_loaded = *(u32 *)0x801CE8C0;
        *(s32 *)((u8 *)record + 0x94) = early_record_time;
        early_loaded = *(u32 *)(early_loaded + 0x828);
        if (early_loaded < (u32)early_record_time) early_latest_time = early_record_time;
        else early_latest_time = early_loaded;
        *(s32 *)((u8 *)record + 0x94) = early_latest_time;
        func_0021D200(0x1C, (s32) early_latest_time, record);
        *(s32 *)((u8 *)record + 0x6C) = (*(s32 *)((u8 *)record + 0x6C) + 1);
        *time_out = *(s32 *)((u8 *)record + 0x94);
        return 0;
    }
    target = func_0020C478(target_index);
    stream_code = func_00201798(*(s32 *)((u8 *)record + 0x48), *(s32 *)((u8 *)record + 0x4C), *(s32 *)((u8 *)record + 0x7C), 0) & 0xFF;
    duration = func_0022257C((u8 *)*(void **)record + 0x44, stream_code, buffer);
    if (func_0020BFE4() != 0) {
        record_time0 = *(s32 *)((u8 *)record + 0x94);
        time = *time_out - buffer[0];
        time = time & ((s32) ~time >> 0x1F);
        if (time < record_time0) {
            time = record_time0;
        }
        target_time0 = *(s32 *)((u8 *)target + 0x94);
        if ((time + buffer[0]) < target_time0) {
            time = target_time0 - buffer[0];
        }
    } else {
        time = *time_out;
        record_time1 = *(s32 *)((u8 *)record + 0x94);
        time = time & ((s32) ~time >> 0x1F);
        if (time < record_time1) {
            time = record_time1;
        }
    }
    selector_id = func_0020C448(record);
    field_code = 0;
    if (selector_id != 0) {
        field_code = func_000454e0(selector_id);
    }
    record_time2 = *(s32 *)((u8 *)record + 0x94);
    if (record_time2 < time) {
        record_time2 = time;
    }
    loaded = *(u32 *)0x801CE8C0;
    *(s32 *)((u8 *)record + 0x94) = record_time2;
    loaded = *(u32 *)(loaded + 0x828);
    if (loaded < (u32)record_time2) latest_time = record_time2;
    else latest_time = loaded;
    *(s32 *)((u8 *)record + 0x94) = latest_time;
    func_0021D200(0x1C, (s32) latest_time, record);
    time = *(s32 *)((u8 *)record + 0x94);
    *(s32 *)((u8 *)record + 0x6C) = (*(s32 *)((u8 *)record + 0x6C) + 1);
    source_value = func_002361EC(record);
    check_value = func_002365BC(record, target, source_value, func_002363FC(target));
    sample0 = rand();
    sample1 = rand();
    check_value = (u32) ((((sample0 << 0x12) & 0x0C000000) | (sample1 << 0xF) | rand()) % 100) < check_value;
    func_0021D230(0x3E, time, record, field_code & 0xFF);
    if (func_0020BFE4() != 0) {
        func_0021D28C(6, time, record, 0xFF, stream_code, *(s32 *)((u8 *)record + 0x7C));
    } else {
        func_0021D28C(6, time, record, 0, 0, *(s32 *)((u8 *)record + 0x7C));
        duration = 0xF;
        buffer[0] = 0xA;
    }
    end_time = time + duration;
    first_time = time + buffer[0];
    if (func_0020BFE4() != 0) {
        source_code0 = func_0020D72C(record);
        func_0021D2C0(0x17, time + buffer[0], 0, 0x5E, field_code & 0xFF, source_code0, func_0020D72C(target));
    }
    value = func_00237750(record, target, 0, 0, 0);
    if (check_value != 0) {
        if (func_0020BFE4() != 0) {
            record58_0 = *(s32 *)((u8 *)record + 0x58);
            target58_0 = *(s32 *)((u8 *)target + 0x58);
            distance = record58_0 - target58_0;
            if (distance <= 0) {
                distance = target58_0 - record58_0;
            }
        } else { distance = 0; }
        check_value = func_00237810(record, target);
        if (check_value != 0) {
            if (func_0020BFE4() != 0) {
                func_0021D200(0x33, (time + buffer[0] + distance) - 0xA, target);
            }
            func_0021D25C(0x34, time + buffer[0] + distance, target, 0xFF, field_code & 0xFF);
            value *= 2;
        }
        if (selector_id != 0) {
            aux_value = func_00045480(selector_id);
        }
        code_byte = field_code & 0xFF;
        if (code_byte == 2) {
            secondary_selector = 2;
        } else {
            secondary_selector = 0 - ((aux_value & 0xFF) != 8);
        }
        if (secondary_selector != -1) {
            secondary_result = func_00237AF8(target, secondary_selector);
        }
        aux_value = 0;
        if (secondary_result == 0) {
            aux_value = func_00237890(record, target);
        }
        action_byte = field_code & 0xFF;
        if (check_value != 0) {
            adjust = func_0022A4E0(target);
        } else {
            adjust = 0;
        }
        func_0022ADFC(target, 0, time + buffer[0] + distance, value, action_byte, aux_value & 0xFF, 1, adjust);
        if (func_0020BFE4() != 0) {
            source_code1 = func_0020D72C(record);
            func_0021D2C0(0x17, (time + buffer[0] + distance) - 3, 0, 0x5D, field_code & 0xFF, source_code1, func_0020D72C(target));
        }
        if ((func_0020C2C0(target) != 0) && (func_0020C014(record) != 0) && (func_0020C104(record) == 0)) {
            increment = func_00233210(record, target);
            if (increment != 0) {
                *(u8 *)((u8 *)record + 0x34) = (*(u8 *)((u8 *)record + 0x34) + increment);
                func_0021D230(0x3F, time + buffer[0] + distance, record, increment);
            }
        }
        if ((secondary_result != 0) && (func_0020C2C0(target) == 0) && (func_0020C32C(target) == 0)) {
            func_0023431C(target, time + buffer[0] + distance, secondary_result);
            target_time2 = *(s32 *)((u8 *)target + 0x94);
            if (end_time < target_time2) {
                end_time = target_time2;
                goto merge_end_time;
            }
        } else {
            goto merge_end_time;
        }
    } else {
        aux_value = func_00201584(*(s32 *)((u8 *)target + 0x48), *(s32 *)((u8 *)target + 0x4C)) & 0xFF;
        if (func_0020BFE4() != 0) {
            record58_1 = *(s32 *)((u8 *)record + 0x58);
            target58_1 = *(s32 *)((u8 *)target + 0x58);
            offset = record58_1 - target58_1;
            if (offset <= 0) {
                offset = target58_1 - record58_1;
            }
        } else { offset = 4; }
        offset = offset - ((func_00201584(*(s32 *)((u8 *)record + 0x48), *(s32 *)((u8 *)record + 0x4C)) & 0xFF) ? 4 : 3);
        check_value = time + buffer[0] + offset;
        func_0021D230(0xE, check_value, target, 0xFF);
        value = func_0022257C((u8 *)*(void **)target + 0x44, ((0 - (aux_value == 0)) & 0x17) | 0x15, 0);
        if ((func_00201584(*(s32 *)((u8 *)target + 0x48), *(s32 *)((u8 *)target + 0x4C)) & 0xFF) && (func_0020BFE4() != 0)) {
            if (func_0020BFF8(target) != 0) {
                event_selector = 0x60;
            } else { event_selector = 0x5F; }
            source_code2 = func_0020D72C(record);
            func_0021D2C0(0x17, check_value, 0, event_selector, field_code & 0xFF, source_code2, func_0020D72C(target));
        }
        func_0021D230(0xF, check_value + value, target, 0xFF);
        bridge_data = (u8 *)*(void **)target + 0x44;
        bridge_selector = 0x18;
        if (aux_value != 0) {
            bridge_selector = 0x16;
        }
        value += func_0022257C(bridge_data, bridge_selector, 0);
        *(s32 *)((u8 *)target + 0x94) = check_value + value;
merge_end_time:
        target_time3 = *(s32 *)((u8 *)target + 0x94);
        if (end_time < target_time3) {
            end_time = target_time3;
        }
    }
    tail_end = end_time - first_time;
    tail_pre = buffer[0];
    *(s32 *)((u8 *)record + 0x88) = tail_end;
    /* These accesses retain the three retail post-store field reloads. */
    tail_duration = *(volatile s32 *)((u8 *)record + 0x88);
    *(s32 *)((u8 *)record + 0x84) = tail_pre;
    tail_first = *(volatile s32 *)((u8 *)record + 0x84);
    tail_last = *(volatile s32 *)((u8 *)record + 0x88);
    result = 1;
    *(s32 *)((u8 *)record + 0x8C) = 0;
    *(s32 *)((u8 *)record + 0x94) = time + tail_duration;
    *time_out = time + tail_first + tail_last;
    return result;
}
