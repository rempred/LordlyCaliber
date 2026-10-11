typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

/* Each retail copy transfers five 16-byte groups: the full 0x50 bytes. */
typedef struct { u32 words[20]; } LocalCopy;
s32 func_00045480(s32);
s32 func_000454e0(s32);
void func_001F0E64(LocalCopy *, u32);
s32 func_001F197C(void *, LocalCopy *, s32 *);
s32 func_00201584(s32, s32);
s32 func_00201798(s32, s32, s32, s32);
s32 func_0020BFE4(void);
s32 func_0020BFF8(void *);
s32 func_0020C014(void *);
s32 func_0020C104(void *);
s32 func_0020C2C0(void *);
s32 func_0020C32C(void *);
s32 func_0020C448(void *);
void *func_0020C478(unsigned int index);
s32 func_0020D72C(void *);
void func_0021D200(s32, s32, void *);
void func_0021D230(s32, s32, void *, s32);
void func_0021D25C(s32, s32, void *, s32, s32);
void func_0021D28C(s32, s32, void *, s32, s32, s32);
void func_0021D2C0(s32, s32, s32, s32, s32, s32, s32);
s32 func_0022222C(void *, void *, s16, s16);
s32 func_002223E0(void *, void *, s32, s32, s32, s32);
s32 func_0022A4E0(void *);
void func_0022ADFC(void *, s32, s32, s32, s32, u8, s32, s32);
s32 func_00233210(void *, void *);
s32 func_00233ED4(void *);
void func_0023431C(void *, s32, s32);
s32 func_002361EC(void *);
s32 func_002363FC(void *);
u32 func_002365BC(void *, void *, s32, s32);
s32 func_00237750(void *, void *, s32, s32, s32);
s32 func_00237810(void *, void *);
s32 func_00237890(void *, void *);
s32 func_00237AF8(void *, s32);
void memset_00023780(s32 *, s32);
s32 rand(void);

s32 func_0022C78C(void *record, s32 *out_time) {
    LocalCopy local_copy;
    s32 local_buffer[6];
    s32 * time_out;
    s32 local_duration;
    s32 local_returnDuration;
    s32 local_start;
    s32 buffer_add;
    u8 local_field57;
    u8 local_field5B;
    s32 local_effect;
    s32 local_hit;
    s32 action_byte, event_time, field57, field5B, tail84, tail88;
    s32 mode0;
    s32 mode1;
    s32 event_selector;
    s32 target_time0;
    s32 early_input_time;
    s32 stream_code1;
    s32 code_byte;
    s32 time0;
    s32 selector0;
    s32 duration0;
    s32 time1;
    s32 source_value;
    u32 sample0;
    s32 time2;
    s32 event_start;
    s32 time3;
    s32 time4;
    s32 time5;
    u32 sample1;
    s32 field_code;
    s32 offset;
    s32 stream_code0;
    s32 target_index;
    s32 increment;
    s32 tail_duration;

    s32 target_time1;
    s32 end_delta;
    s32 secondary_selector;
    s32 bridge_selector;
    s32 aux_value;
    s32 time;
    s32 end_time;
    s32 record_time;
    s32 early_record_time;
    s32 event_delta;
    s32 candidate_end_time, miss_mask;
    s32 adjust;
    u32 check_value;
    u32 loaded, early_loaded, allocation_value;
    register u32 latest_time;
    register u32 early_latest_time;
    void *entry0;
    void *entry1;
    void *target;
    time = 0;
    time_out = out_time;
    local_effect = 0;
    local_returnDuration = 0;
    memset_00023780(local_buffer, 0x18);
    target_index = func_00233ED4(record);
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
    local_field57 = *(u8 *)((u8 *)target + 0x57);
    local_field5B = *(u8 *)((u8 *)target + 0x5B);
    source_value = func_002361EC(record);
    check_value = func_002365BC(record, target, source_value, func_002363FC(target));
    sample0 = rand();
    sample1 = rand();
    check_value = (u32) ((((sample0 << 0x12) & 0x0C000000) | (sample1 << 0xF) | rand()) % 100) < check_value;
    local_copy = *(LocalCopy *)((u8 *)*(void **)(record) + 0x44);
    mode0 = 0;
    if (check_value == 0) {
        if (func_00201584(*(s32 *)((u8 *)target + 0x48), *(s32 *)((u8 *)target + 0x4C)) & 0xFF) {
            mode0 = 2;
        } else {
            mode0 = 1;
        }
    }
    stream_code0 = func_00201798(*(s32 *)((u8 *)record + 0x48), *(s32 *)((u8 *)record + 0x4C), *(s32 *)((u8 *)record + 0x7C), mode0);
    func_001F0E64(&local_copy, stream_code0 & 0xFF);
    func_001F197C(record, &local_copy, local_buffer);
    if (func_0020BFE4() != 0) {
        entry0 = (*(void **)((u8 *)(record) + (4)));
        time = func_0022222C(record, target, *(s16 *)((u8 *)entry0 + 0x1C), *(s16 *)((u8 *)entry0 + 0x20));
    } else {
        local_buffer[0] = 0;
    }
    offset = time + local_buffer[0];
    record_time = *time_out;
    time = record_time - offset;
    if (time < 0) time = 0;
    record_time = *(s32 *)((u8 *)record + 0x94);
    if (time < record_time) {
        time = record_time;
    }
    target_time0 = *(s32 *)((u8 *)target + 0x94);
    if ((time + offset) < target_time0) {
        time = target_time0 - offset;
    }
    if (record_time < time) {
        record_time = time;
    }
    loaded = *(u32 *)0x801CE8C0;
    *(s32 *)((u8 *)record + 0x94) = record_time;
    loaded = *(u32 *)(loaded + 0x828);
    if (loaded < (u32)record_time) latest_time = record_time;
    else latest_time = loaded;
    *(s32 *)((u8 *)record + 0x94) = latest_time;
    func_0021D200(0x1C, (s32) latest_time, record);
    time = *(s32 *)((u8 *)record + 0x94);
    *(s32 *)((u8 *)record + 0x6C) = (*(s32 *)((u8 *)record + 0x6C) + 1);
    if (func_0020BFE4() != 0) {
        func_0021D25C(0x1D, time, record, 1, stream_code0 & 0xFF);
        func_0021D25C(0x1D, time, record, 2, 7);
        func_0021D28C(2, time + local_buffer[0], record, 2, *(s32 *)((u8 *)record + 0x68), 5);
    }
    entry1 = (*(void **)((u8 *)(record) + (4)));
    local_copy = *(LocalCopy *)((u8 *)*(void **)((u8 *)record + 4) + 0x44);
    mode1 = 0;
    if (check_value == 0) {
        if (func_00201584(*(s32 *)((u8 *)target + 0x48), *(s32 *)((u8 *)target + 0x4C)) & 0xFF) {
            mode1 = 2;
        } else {
            mode1 = 1;
        }
    }
    time2 = time + offset;
    stream_code1 = func_00201798(*(s32 *)((u8 *)record + 0x48), *(s32 *)((u8 *)record + 0x4C), *(s32 *)((u8 *)record + 0x7C), mode1) & 0xFF;
    func_001F0E64(&local_copy, stream_code1);
    local_duration = func_001F197C(record, &local_copy, local_buffer);
    if (func_0020BFE4() != 0) {
        if (check_value != 0) {
            func_0021D28C(6, time2, record, 2, stream_code1, *(s32 *)((u8 *)record + 0x7C));
        } else {
            func_0021D28C(7, time2, record, 2, stream_code1, *(s32 *)((u8 *)record + 0x7C));
        }
    } else {
        if (check_value != 0) {
            func_0021D28C(6, time2, record, 0, 0, *(s32 *)((u8 *)record + 0x7C));
        } else {
            func_0021D28C(7, time2, record, 0, 0, *(s32 *)((u8 *)record + 0x7C));
        }
        local_duration = 0x14;
        local_buffer[0] = 0xF;
    }
    field_code = func_000454e0(func_0020C448(record) & 0xFFFF);
    buffer_add = local_buffer[0];
    end_time = time2;
    end_time += local_duration;
    /* Unsigned negation/subtraction retains the word-sized sum and pinned
     * GCC operand order. Direct sums commute the addu at retail +0x450. */
    buffer_add = (s32)(0U - (u32)buffer_add);
    buffer_add = (s32)((u32)time2 - (u32)buffer_add);
    event_start = buffer_add - 5;
    local_start = time2;
    if (check_value != 0) {
        code_byte = field_code & 0xFF;
        /* Pinned GCC allocation: the equivalent simpler form changes retail
         * register allocation (15 native word differences). */
        allocation_value = (u32)code_byte | (u32)event_start;
        code_byte = allocation_value & (u32)code_byte;
        func_0021D230(0x3E, time, record, code_byte);
        if (func_0020BFE4() != 0) {
            func_0021D2C0(0x17, event_start, 0, 0x5D, code_byte, func_0020D72C(record), *(s32 *)((u8 *)record + 0x68));
        }
        if (event_start < local_start) {
            local_start = event_start;
        }
        if (func_0020BFE4() != 0) {
            event_delta = 0x19;
        } else {
            event_delta = 5;
        }
        candidate_end_time = event_delta + event_start;
        goto merge_event_end;
    }
    if (func_00201584(*(s32 *)((u8 *)target + 0x48), *(s32 *)((u8 *)target + 0x4C)) & 0xFF) {
        if (func_0020BFF8(target) != 0) {
            event_selector = 0x60;
        } else {
            event_selector = 0x5F;
        }
        if (func_0020BFE4() != 0) {
            func_0021D2C0(0x17, event_start, 0, event_selector, field_code & 0xFF, func_0020D72C(record), *(s32 *)((u8 *)record + 0x68));
        }
        if (event_start < local_start) {
            local_start = event_start;
        }
        miss_mask = 0xF;
        if (func_0020BFE4() == 0) miss_mask = 0;
        miss_mask |= 5;
        candidate_end_time = event_start + miss_mask;
merge_event_end:
        if (end_time < candidate_end_time) {
            end_time = candidate_end_time;
        }
    }
    if (check_value != 0) {
        local_hit = func_00237810(record, target);
        check_value = func_00237750(record, target, 0, 0, 0);
        if (local_hit != 0) {
            if (func_0020BFE4() != 0) {
                func_0021D200(0x33, (time + offset + local_buffer[0]) - 0xA, target);
            }
            func_0021D25C(0x34, time + offset + local_buffer[0], target, 0xFF, field_code & 0xFF);
            check_value *= 2;
        }
        selector0 = func_00045480(func_0020C448(record) & 0xFFFF);
        if ((field_code & 0xFF) == 2) {
            secondary_selector = 2;
        } else {
            secondary_selector = 0 - ((selector0 & 0xFF) != 8);
        }
        if (secondary_selector != -1) {
            local_effect = func_00237AF8(target, secondary_selector);
        }
        aux_value = 0;
        if (local_effect == 0) {
            aux_value = func_00237890(record, target);
        }
        action_byte = field_code & 0xFF;
        if (local_hit != 0) {
            adjust = func_0022A4E0(target);
        } else {
            adjust = 0;
        }
        time0 = time + offset;
        /* Pinned GCC allocation: the equivalent simpler form changes retail
         * register allocation (42 native word differences). */
        allocation_value = check_value | (u32)end_time;
        check_value = allocation_value & check_value;
        func_0022ADFC(target, 0, time0 + local_buffer[0], check_value, action_byte, aux_value & 0xFF, 1, adjust);
        if ((func_0020C2C0(target) != 0) && (func_0020C014(record) != 0) && (func_0020C104(record) == 0)) {
            increment = func_00233210(record, target);
            if (increment != 0) {
                event_time = time0 + local_duration;
                *(u8 *)((u8 *)record + 0x34) = (*(u8 *)((u8 *)record + 0x34) + increment);
                func_0021D230(0x3F, event_time, record, increment);
            }
        }
        if ((local_effect != 0) && (func_0020C2C0(target) == 0) && (func_0020C32C(target) == 0)) {
            func_0023431C(target, time + offset + local_duration, local_effect);
            goto merge_target_end;
        }
    } else {
        selector0 = func_00201584(*(s32 *)((u8 *)target + 0x48), *(s32 *)((u8 *)target + 0x4C)) & 0xFF;
        time3 = time + offset + local_buffer[0];
        local_copy = *(LocalCopy *)((u8 *)*(void **)(target) + 0x44);
        func_001F0E64(&local_copy, ((0 - (selector0 == 0)) & 0x17) | 0x15);
        check_value = func_001F197C(target, &local_copy, 0);
        func_0021D230(0xE, time3, target, 0xFF);
        time4 = time3 + check_value;
        local_copy = *(LocalCopy *)((u8 *)*(void **)(target) + 0x44);
        bridge_selector = 0x18;
        if (selector0 != 0) {
            bridge_selector = 0x16;
        }
        func_001F0E64(&local_copy, bridge_selector);
        duration0 = func_001F197C(target, &local_copy, 0);
        func_0021D230(0xF, time4, target, 0xFF);
        check_value += duration0;
        candidate_end_time = time4 + check_value;
        if (*(s32 *)((u8 *)target + 0x94) < candidate_end_time) {
            *(s32 *)((u8 *)target + 0x94) = candidate_end_time;
        }
merge_target_end:
        target_time1 = *(s32 *)((u8 *)target + 0x94);
        if (end_time < target_time1) {
            end_time = target_time1;
        }
    }
    time1 = time + offset;
    /* Pinned GCC allocation: the equivalent simpler form changes retail
     * register allocation (5 native word differences). */
    allocation_value = (u32)time1 | (u32)local_duration;
    time1 = allocation_value & (u32)time1;
    /* Pinned GCC allocation: the equivalent simpler form changes retail
     * register allocation (5 native word differences). */
    allocation_value = (u32)time1 | (u32)local_start;
    time1 = allocation_value & (u32)time1;
    time5 = time1 + local_duration;
    if (func_0020BFE4() != 0) {
        field57 = local_field57;
        field5B = local_field5B;
        local_returnDuration = func_002223E0(record, target, field57, field5B, (s32) (*(s16 *)((u8 *)((*(void **)((u8 *)(record) + (4)))) + (0x1C))), (s32) (*(s16 *)((u8 *)((*(void **)((u8 *)(record) + (4)))) + (0x20))));
        func_0021D25C(8, time5, record, 2, 0xB);
    }
    func_0021D25C(9, time5 + local_returnDuration, record, 2, 0xC);
    *(s32 *)((u8 *)record + 0x84) = offset;
    end_delta = end_time - local_start;
    *(s32 *)((u8 *)record + 0x88) = end_delta;
    tail_duration = (local_duration + local_returnDuration) - end_delta;
    *(s32 *)((u8 *)record + 0x8C) = tail_duration;
    if (tail_duration < 0) {
        *(s32 *)((u8 *)record + 0x8C) = 0;
    }
    tail84 = *(s32 *)((u8 *)record + 0x84);
    tail88 = *(s32 *)((u8 *)record + 0x88);
    time5 = local_returnDuration;
    *(s32 *)((u8 *)record + 0x94) = (time1 + local_duration + time5);
    *time_out = time + tail84 + tail88;
    return 1;
}
