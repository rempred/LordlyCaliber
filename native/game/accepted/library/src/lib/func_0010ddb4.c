typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef s32 M2C_UNK;
typedef s8 M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;
s32 func_00022e60();
void func_0002de50(void *, void *, s32);
void func_0005c090(s32, s32);
M2C_UNK func_000ec838(M2C_UNK, s32, M2C_UNK, M2C_UNK, s32, s32, s32, s32, s32);
void func_000f6b10(void *, f32 *, f32 *, f32 *);
void func_0010746C(void *);
M2C_UNK func_00109C04();
M2C_UNK func_00109C3C(void *);
f32 func_00118500(void *, void *);
M2C_UNK func_0011BB4C();
M2C_UNK func_00129948(void *);
s32 func_0012DA10(void *);
s32 func_0012DA9C(void *);
void *func_0012E7E8(void *);
s32 func_0012E968(void *);
void func_0012EA80(s32, f32 *);
M2C_UNK func_0012F140(void *);
s32 func_0012F530(void *);
M2C_UNK func_0012F8C0(void *);
s32 func_0012FBB8(s32, s32);
M2C_UNK func_00130E60(u16, M2C_UNK);
M2C_UNK func_00131050();
M2C_UNK func_0013109C(u16);
M2C_UNK func_00132B80(u16);
M2C_UNK func_00147BDC(s32 *);
M2C_UNK func_0015DF10(M2C_UNK);
void func_800EA604(void *, s32);
s32 func_801DCF90(void *);
s32 func_801DD2B0(void *);
void memset_00023780(void *, s32);
void os_inval_dcache(void *, s32);
void os_inval_icache(void *, s32);

extern volatile f32 D_801F0ADC;
extern f32 D_801F0AD0;
extern f32 D_801F0AD4;
extern f32 D_801F0AD8;
extern volatile f32 D_801F0AE0;
extern volatile f32 D_801F0AE4;
extern f32 D_801F0AE8;
extern f32 D_801F0AEC;
extern f32 D_801F0AF0;
extern f32 D_801F0B80;
extern f32 D_801F0D8C;
extern f32 D_801F3604;
extern f32 D_801F3664;
extern f32 D_801F3668;
extern f32 D_801F366C;
extern f64 D_801EE2E0;
extern f64 D_801EE2E8;
extern s32 D_801F0CA4;
extern s32 *D_801F0CB0[];
extern s32 D_801F0DE0;
extern s32 D_801F0DE8;
extern s32 D_801F0DEC;
extern s32 D_801F0DF0;
extern s32 D_801F0DFC;
extern s32 D_801F0E00;
/* Coherent ordinary-word source hypothesis for all existing accesses.
 * An ordinary argument load can combine into its hard argument register;
 * no asynchronous-state claim follows from this private trial. */
extern s32 D_801F0E04;
/* Existing four-byte word view for the first late read.  The real indexed
 * halfword update must invalidate this value before the later direct read.
 * This adds no field beyond that word and no volatile or storage effect. */
typedef struct Func0010DDB4LateIndexWord { s32 value; } Func0010DDB4LateIndexWord;
extern u16 D_801F0FE4[];
extern u8 D_801F1024[];
extern u8 D_801F0FDF;
extern u8 D_801F0FE0;
extern u8 D_801F0FE1;
extern s32 D_801F3610;
extern u8 D_801F3600;
/* Retail delay-slot selection crosses the store to this actual word.
 * Model its accesses as ordinary; a volatile resource blocks that motion.
 * This is static source reconstruction, with no asynchronous-state claim. */
extern s32 D_801F3658;
extern u8 D_801F365D;
extern s8 D_801F3674;
extern volatile s8 D_801F3680;
extern u16 D_801F36A2;
extern u16 D_801F36D0;
extern void *D_8018F500;
extern s32 D_8018F504;
extern u8 D_801951B0[];
extern void *D_801AB850;
extern s32 D_801F3A30;
extern s32 D_801F0BA8;
extern s8 D_801F1039[];
extern f32 D_801F0D98;
extern f32 D_801F0D9C;
extern f32 D_801F0DA0;
extern f32 D_801F0DA4;
extern u8 D_801969B9[];
extern u8 D_801969BA[];
extern u8 D_801969B8[];
extern u8 D_80196A38[];
extern u8 D_8018F520[];
extern u8 D_8018F481;
extern u8 D_800E9C16;
extern u8 g_func_0015DF10_match[];
extern u8 D_00171EA0[];
extern u8 D_00177ED0[];
extern u8 D_0017F9C0[];
extern u8 D_00188B60[];
extern u8 D_0018F100[];
extern u8 D_001A2C20[];
extern u8 D_001A4C10[];
extern u8 D_80214F80[];
extern u8 D_802167E0[];
extern u8 D_80216F70[];
extern u8 D_80216F90[];
extern u8 D_802178B0[];
extern u8 D_80219010[];
extern u8 D_8021AFB0[];
extern u8 D_8021AFF0[];
extern u8 D_8021B470[];
extern u8 D_8021B520[];
extern u8 D_8021B5F0[];
extern u8 D_8021E120[];
extern u8 D_8021E130[];
extern u8 overlay_a_icache_text_start[];
extern u8 overlay_a_icache_data_start[];
extern u8 overlay_a_dcache_data_start[];
extern u8 overlay_a_dcache_bss_start[];
extern u8 overlay_a_dma_rom_start[];
extern u8 overlay_a_dma_rom_end[];
extern u8 overlay_a_dma_text_start[];
extern u8 overlay_a_bss_start[];
extern u8 overlay_a_bss_end[];
extern u8 overlay_b_icache_text_start[];
extern u8 overlay_b_icache_data_start[];
extern u8 overlay_b_dcache_data_start[];
extern u8 overlay_b_dcache_bss_start[];
extern u8 overlay_b_dma_rom_start[];
extern u8 overlay_b_dma_rom_end[];
extern u8 overlay_b_dma_text_start[];
extern u8 overlay_b_bss_start[];
extern u8 overlay_b_bss_end[];
extern u8 overlay_188b60_icache_text_start[];
extern u8 overlay_188b60_icache_data_start[];
extern u8 overlay_188b60_dcache_data_start[];
extern u8 overlay_188b60_dcache_bss_start[];
extern u8 overlay_188b60_dma_rom_start[];
extern u8 overlay_188b60_dma_rom_end[];
extern u8 overlay_188b60_dma_text_start[];
extern u8 overlay_188b60_bss_start[];
extern u8 overlay_188b60_bss_end[];
extern u8 overlay_17f9c0_icache_text_start[];
extern u8 overlay_17f9c0_icache_data_start[];
extern u8 overlay_17f9c0_dcache_data_start[];
extern u8 overlay_17f9c0_dcache_bss_start[];
extern u8 overlay_17f9c0_dma_rom_start[];
extern u8 overlay_17f9c0_dma_rom_end[];
extern u8 overlay_17f9c0_dma_text_start[];
extern u8 overlay_17f9c0_bss_start[];
extern u8 overlay_17f9c0_bss_end[];

typedef struct Func0010DDB4Transform {
    u8 pad_00[0x14];
    f32 field_14;
    f32 field_18;
    f32 field_1C;
    f32 field_20;
    f32 field_24;
    f32 field_28;
} Func0010DDB4Transform;




/* Both real BSS paths return the word address consumed by the halfword read.
 * Taking this handle directly, without first assigning the caller's pointer,
 * keeps its two return copies through both CSE passes. They coalesce into s0
 * and disappear in the late jump pass, retaining the retail indexed read. */
static __inline__ s32 *prepare_case02_overlay(s32 *word, s32 value)
{
    *(volatile s32 *)word = value;
    os_inval_icache(overlay_a_icache_text_start, overlay_a_icache_data_start - overlay_a_icache_text_start);
    os_inval_dcache(overlay_a_dcache_data_start, overlay_a_dcache_bss_start - overlay_a_dcache_data_start);
    func_0002de50(overlay_a_dma_rom_start, overlay_a_dma_text_start, overlay_a_dma_rom_end - overlay_a_dma_rom_start);
    if (overlay_a_bss_start == overlay_a_bss_end) {
        return word;
    }
    memset_00023780(overlay_a_bss_start, overlay_a_bss_end - overlay_a_bss_start);
    return word;
}

/* One pointer-sized view of the existing word, with no adjacent storage.
 * For this compiler it retains the store dependency of the following indexed
 * halfword read while allowing that read to move across the scalar message store.
 * This is a compiler workaround, not a claim about an original aggregate. */
typedef struct {
    void *value;
} Func0010DDB4ExistingPointerWord;

s32 func_0010DDB4(Func0010DDB4Transform *arg0) {
    f32 output_position[3];
    f32 projected_position[2];
    s32 *current_object;
    s32 state_value;
    s32 state_four;
    s32 case17_selector_or_flags;
    s32 case17_index_or_flags;
    s32 case17_state_or_value;
    register f32 temp_f10_92;
    f32 temp_f2_683;
    register f32 temp_f4_73;
    register f32 initial_zero;
    f32 temp_f0_1329;
    register f32 var_f12_0;
    register f32 normalized_x;
    register f32 normalized_z;
    register f32 child_normalized_x;
    f32 normalization_work;
    register f32 child_x_base;
    register f32 child_z_base;
    register f32 object_scale;
    register f32 object_height;
    register f32 *state_x;
    f32 axis_delta;
    register f32 axis_zero;
    register f32 axis_value;
    f32 clamp_work;
    register f32 transformed_x;
    register f32 transformed_z;
    register f32 var_f2_275;
    register f32 var_f2_321;
    register f32 var_f2_367;
    register s32 **var_s1_641;
    register s32 cleanup_mask;
    register u8 *child_loop_end;
    s32 temp_a2_661;
    s32 temp_v0_1157;
    s32 temp_v1_1299;
    s32 temp_v1_1563;
    s32 temp_v1_1641;
    s32 temp_v1_500;
    s32 temp_v1_985;
    register s32 primary_object_flags;
    s32 primary_loaded_header;
    s32 primary_first_masked_header;
    s32 primary_second_masked_header;
    s32 object_header_word;
    register s32 initial_object_flags_v1;
    register s32 initial_object_mask_v0;
    register s32 child_record_index;
    register s32 child_record_byte;
    register s32 child_record_offset;
    register u8 *child_record_list;
    register void *primary_object_arg;
    register void *child_object_arg;
    register s32 child_flag_mask;
    register s32 selected_record_index;
    register s32 **selected_record_slot;
    s32 *state_base_s1;
    s32 var_a0_615;
    register s32 var_s2_638;
    register void **record_table_s4;
    register s32 var_s5_11;
    register s32 var_v1_2224;
    s32 var_v1_23;
    register s32 var_v0_1810;
    register s32 var_v0_1818;
    register s32 var_v0_468;
    u16 var_v0_1730;
    register s32 var_v0_1977;
    u32 temp_v1_420;
    register u8 *var_s1_745;
    register s32 final_record_index_v0;
    s32 case2d_slot_address;
    void *final_record;
    register s32 final_result_v0;
    register s32 final_v1;
    s32 case1f_mask_work;
    register s32 final_a0;
    register s32 case11_index_v1;
    register s32 case11_offset_v0;
    void *case11_result_v0;
    s32 case11_integer_word;
    s32 case11_field_address;
    register s32 case11_record_index_a0;
    register s32 case11_record_index_a1;
    register s32 case11_offset_v1;
    register s32 shared_message_state_v0;
    void *shared_record_call_argument;
    register s32 case13_index;
    register s32 case2e_index_v1;
    register s32 case2e_record_index_v0;
    register s32 case2e_state_v1;
    register u8 case2e_flags_v1;
    register s32 case2e_mode_a0;
    register s32 **case1e_table_s1;
    s32 case1e_record_index;
    register s32 case1e_v0;
    register s32 case1e_v1;
    register u8 *case1e_list_a1;
    register s32 case1e_loop_a0;
    register s32 case1e_mask_a2;
    register s32 **case1e_table_a3;
    register s32 case1e_ff_t0;
    register s32 case1f_index_v0;
    register s32 case22_index_v0;
    register s32 case22_flags_v0;
    register s32 case22_mask_v1;
    register s32 **case22_table_s1;
    register s32 case22_state_v0;
    register s32 case22_object_flags_v1;
    register s32 case22_minus_one_v0;
    register s32 case23_index_v0;
    register s32 case23_mask_a1;
    register s32 case23_loaded_flag_byte;
    register s32 case23_global_v1;
    register s32 case23_flags_a0;
    register s32 case23_minus_one_v0;
    register s32 case25_mask_v1;
    register s32 case25_flags_v0;
    register s32 case32_flags_v1;
    register s32 case32_mask_v0;
    register s32 case27_index_v0;
    register s32 case27_mask_a1;
    register u8 case27_global_v0;
    register s32 case27_flags_v1;
    register s32 class_search_v1;
    u8 temp_a1_571;
    u8 temp_v1_752;
    register s32 var_a1_2356;
    register s32 var_s1_1877;
    register u8 var_v0_477;
    register u8 var_v0_619;
    register u8 var_v1_548;
    /* Actual address then 0/2 flag, with their disjoint working lifetimes. */
    register u32 case0f_secondary_word;
    s32 case0f_helper_word;
    register void *temp_s2_734;
    void *temp_v0_411;
    void *var_a0_1646;
    void *var_a0_821;

    var_s5_11 = 0;
    var_v1_23 = 0;
    if (D_801F0ADC != 0.0f) {
        goto block_7;
    }
    if (D_801F0AE0 != 0.0f) {
        goto block_7;
    }
    if (D_801F0AE4 != 0.0f) {
        goto block_7;
    }
    state_x = &D_801F3664;
    if (*state_x != D_801F0AE8) {
        goto block_7;
    }
    if (D_801F3668 != D_801F0AEC) {
        goto block_7;
    }
    if (D_801F366C != D_801F0AF0) {
        goto block_7;
    }
    var_s5_11 = 1;
    *state_x = D_801F0AE8;
    D_801F3668 = D_801F0AEC;
    D_801F366C = D_801F0AF0;
block_7:
    if (var_s5_11 != 0) {
        goto block_56;
    }
    /* Preserve the original motion deltas before advancing the three fields. */
    initial_zero = 0.0f;
    temp_f4_73 = arg0->field_20 - arg0->field_14;
    temp_f10_92 = arg0->field_24 - arg0->field_18;
    var_f12_0 = arg0->field_28 - arg0->field_1C;
    arg0->field_20 += D_801F0ADC;
    arg0->field_24 += D_801F0AE0;
    arg0->field_28 += D_801F0AE4;
    if (D_801F0D8C != initial_zero) {
        goto block_12;
    }
    if (D_801F3604 != initial_zero) {
        goto block_12;
    }
    if (D_801F0B80 != initial_zero) {
        goto block_12;
    }
    D_801F3664 = D_801F0AE8;
    D_801F3668 = D_801F0AEC;
    D_801F366C = D_801F0AF0;
    var_v1_23 += 3;
    goto block_33;
block_12:
    
    state_x = &D_801F3664;
    axis_value = *state_x;
    axis_delta = D_801F0D8C;
    axis_value += axis_delta;
    *state_x = axis_value;
    axis_zero = 0.0f;
    if (axis_delta != axis_zero) {
        goto block_14;
    }
    var_v1_23 += 1;
    D_801F0D8C = axis_zero;
    *state_x = D_801F0AE8;
    goto block_19;
block_14:
    if (axis_zero < axis_delta) {
        if (D_801F0AE8 <= axis_value) {
            var_v1_23 += 1;
            *state_x = D_801F0AE8;
            D_801F0D8C = axis_zero;
        }
    } else if (axis_value < D_801F0AE8) {
        var_v1_23 += 1;
        *state_x = D_801F0AE8;
        D_801F0D8C = axis_zero;
    }
block_19:
    state_x = &D_801F3668;
    axis_value = *state_x;
    axis_delta = D_801F3604;
    axis_value += axis_delta;
    *state_x = axis_value;
    axis_zero = 0.0f;
    if (axis_delta != axis_zero) {
        goto block_21;
    }
    var_v1_23 += 1;
    D_801F3604 = axis_zero;
    *state_x = D_801F0AEC;
    goto block_26;
block_21:
    if (axis_zero <= axis_delta) {
        if (D_801F0AEC <= axis_value) {
            var_v1_23 += 1;
            *state_x = D_801F0AEC;
            D_801F3604 = axis_zero;
        }
    } else if (axis_value < D_801F0AEC) {
        var_v1_23 += 1;
        *state_x = D_801F0AEC;
        D_801F3604 = axis_zero;
    }
block_26:
    state_x = &D_801F366C;
    axis_value = *state_x;
    axis_delta = D_801F0B80;
    axis_value += axis_delta;
    *state_x = axis_value;
    axis_zero = 0.0f;
    if (axis_delta != axis_zero) {
        goto block_28;
    }
    var_v1_23 += 1;
    D_801F0B80 = axis_zero;
    *state_x = D_801F0AF0;
    goto block_33;
block_28:
    if (axis_zero <= axis_delta) {
        if (D_801F0AF0 <= axis_value) {
            var_v1_23 += 1;
            *state_x = D_801F0AF0;
            D_801F0B80 = axis_zero;
        }
    } else if (axis_value < D_801F0AF0) {
        var_v1_23 += 1;
        *state_x = D_801F0AF0;
        D_801F0B80 = axis_zero;
    }
block_33:
    clamp_work = D_801F0ADC;
    var_f2_275 = 0.0f;
    if (clamp_work == var_f2_275) {
        var_v1_23 += 1;
        arg0->field_20 = D_801F0AD0;
    } else if (((var_f2_275 < clamp_work) &&
                (clamp_work = arg0->field_20, var_f2_275 = D_801F0AD0,
                 var_f2_275 <= clamp_work)) ||
               ((D_801F0ADC < 0.0f) &&
                (clamp_work = arg0->field_20, var_f2_275 = D_801F0AD0,
                 clamp_work <= var_f2_275))) {
        arg0->field_20 = var_f2_275;
        var_v1_23 += 1;
    }
block_40:
    clamp_work = D_801F0AE0;
    var_f2_321 = 0.0f;
    if (clamp_work == var_f2_321) {
        var_v1_23 += 1;
        arg0->field_24 = D_801F0AD4;
    } else if (((var_f2_321 < clamp_work) &&
                (clamp_work = arg0->field_24, var_f2_321 = D_801F0AD4,
                 var_f2_321 <= clamp_work)) ||
               ((D_801F0AE0 < 0.0f) &&
                (clamp_work = arg0->field_24, var_f2_321 = D_801F0AD4,
                 clamp_work <= var_f2_321))) {
        arg0->field_24 = var_f2_321;
        var_v1_23 += 1;
    }
block_47:
    clamp_work = D_801F0AE4;
    var_f2_367 = 0.0f;
    if (clamp_work == var_f2_367) {
        var_v1_23 += 1;
        arg0->field_28 = D_801F0AD8;
    } else if (((var_f2_367 < clamp_work) &&
                (clamp_work = arg0->field_28, var_f2_367 = D_801F0AD8,
                 var_f2_367 <= clamp_work)) ||
               ((D_801F0AE4 < 0.0f) &&
                (clamp_work = arg0->field_28, var_f2_367 = D_801F0AD8,
                 clamp_work <= var_f2_367))) {
        arg0->field_28 = var_f2_367;
        var_v1_23 += 1;
    }
block_54:
    arg0->field_14 = arg0->field_20 - temp_f4_73;
    (arg0->field_18) = (f32) ((arg0->field_24) - temp_f10_92);
    (arg0->field_1C) = (f32) ((arg0->field_28) - var_f12_0);
    if (var_v1_23 != 6) {
        goto block_56;
    }
    var_s5_11 = 1;
block_56:
    func_000f6b10(arg0, &D_801F3664, &projected_position[0], &projected_position[1]);
    temp_v0_411 = D_801AB850;
    
    transformed_x = projected_position[0];
    transformed_z = projected_position[1];
    
    (*(f32 *)((s8 *)(temp_v0_411) + (8))) = transformed_x;
    
    (*(f32 *)((s8 *)(temp_v0_411) + (0xC))) = transformed_z;
    if (var_s5_11 != 1) {
        goto block_227;
    }
    
    
    
    
    
    temp_v1_420 = D_801F0CA4;
    D_801F3610 = 0;
    D_801F3600 = 0;
    if (temp_v1_420 >= 0x34U) {
        goto block_227;
    }
    switch (temp_v1_420) {
    case 0x00: goto dispatch_case_00_2f;
    case 0x2f: goto dispatch_case_00_2f;
    case 0x01: goto dispatch_case_01;
    case 0x02: goto dispatch_case_02;
    case 0x15: goto dispatch_case_15;
    case 0x0b: goto dispatch_case_0b;
    case 0x2b: goto dispatch_case_2b;
    case 0x11: goto dispatch_case_11;
    case 0x0c: goto dispatch_case_0c;
    case 0x12: goto dispatch_case_12;
    case 0x13: goto dispatch_case_13;
    case 0x16: goto dispatch_case_16;
    case 0x17: goto dispatch_case_17;
    case 0x18: goto dispatch_case_18;
    case 0x19: goto dispatch_case_19;
    case 0x0f: goto dispatch_case_0f;
    case 0x2e: goto dispatch_case_2e;
    case 0x1a: goto dispatch_case_1a;
    case 0x1b: goto dispatch_case_1b;
    case 0x1e: goto dispatch_case_1e;
    case 0x1f: goto dispatch_case_1f;
    case 0x21: goto dispatch_case_21;
    case 0x20: goto dispatch_case_20;
    case 0x22: goto dispatch_case_22;
    case 0x23: goto dispatch_case_23;
    case 0x25: goto dispatch_case_25;
    case 0x26: goto dispatch_case_26;
    case 0x32: goto dispatch_case_32;
    case 0x27: goto dispatch_case_27;
    case 0x28: goto dispatch_case_28;
    case 0x29: goto dispatch_case_29;
    case 0x2a: goto dispatch_case_2a;
    case 0x2c: goto dispatch_case_2c;
    case 0x2d: goto dispatch_case_2d;
    case 0x30: goto dispatch_case_30;
    case 0x31: goto dispatch_case_31;
    case 0x33: goto dispatch_case_33;
    case 0x03: goto dispatch_default;
    case 0x04: goto dispatch_default;
    case 0x05: goto dispatch_default;
    case 0x06: goto dispatch_default;
    case 0x07: goto dispatch_default;
    case 0x08: goto dispatch_default;
    case 0x09: goto dispatch_default;
    case 0x0a: goto dispatch_default;
    case 0x0d: goto dispatch_default;
    case 0x0e: goto dispatch_default;
    case 0x10: goto dispatch_default;
    case 0x14: goto dispatch_default;
    case 0x1c: goto dispatch_default;
    case 0x1d: goto dispatch_default;
    case 0x24: goto dispatch_default;
    default: goto dispatch_default;
    }
dispatch_case_00_2f:
    func_00109C04();
    selected_record_index = D_801F3658;
    state_four = 4;
    D_801F0DE0 = state_four;
    selected_record_slot = &D_801F0CB0[selected_record_index];
    D_801F0FE1 = func_0012F530(*selected_record_slot);
    D_801F0FDF = 1;
    if (D_801F0CA4 != 0) {
        goto block_61;
    }
    D_801F0FE0 = 0;
    goto block_64;
block_61:
    
    var_v0_468 = *D_801F0CB0[D_801F3658] & 0x40;
    if (var_v0_468 != 0) {
        var_v0_468 = 2;
        goto block_63;
    }
    var_v0_468 = 3;
block_63:
    
    D_801F0FE0 = var_v0_468;
block_64:
    var_v0_477 = D_801F365D | 1;
    goto block_212;
dispatch_case_01:
    selected_record_index = D_801F3658;
    state_four = 4;
    D_801F0DE0 = state_four;
    selected_record_slot = &D_801F0CB0[selected_record_index];
    D_801F0FE1 = func_0012F530(*selected_record_slot);
    var_v1_548 = D_801F365D;
    D_801F0FDF = 0;
    D_801F0FE0 = 0;
    goto block_117;
dispatch_case_02:
    temp_v1_500 = D_801F3A30;
    if (temp_v1_500 < 0x64) {
        goto block_70;
    }
    /* This reused object address performs byte arithmetic only; both actual accesses below retain volatile word/halfword lvalues. */
    current_object = prepare_case02_overlay(&D_801F0BA8, temp_v1_500 - 0x64);
    func_0013109C(*(volatile u16 *)((u8 *)current_object + 2));
    D_801F0DF0 = 0;
    D_801F0DE0 = 0xA;
    var_v1_548 = D_801F365D;
    goto block_117_flags;
block_70:
    current_object = D_801F0CB0[temp_v1_500];
    state_base_s1 = &D_801F0DE0;
    D_801F3658 = temp_v1_500;
    *state_base_s1 = 4;
    D_801F0FE1 = func_0012F530(current_object);
    D_801F0FE0 = 0;
    D_801F0FDF = 0;
    D_801F0DEC = 0;
    D_801F0DF0 = 0;
    temp_a1_571 = (*(u8 *)((s8 *)(current_object) + (4)));
    if (temp_a1_571 < 0x1EU) {
        goto block_74;
    }
    os_inval_icache(overlay_188b60_icache_text_start, overlay_188b60_icache_data_start - overlay_188b60_icache_text_start);
    os_inval_dcache(overlay_188b60_dcache_data_start, overlay_188b60_dcache_bss_start - overlay_188b60_dcache_data_start);
    func_0002de50(overlay_188b60_dma_rom_start, overlay_188b60_dma_text_start, overlay_188b60_dma_rom_end - overlay_188b60_dma_rom_start);
    if (overlay_188b60_bss_start == overlay_188b60_bss_end) {
        goto block_73;
    }
    memset_00023780(overlay_188b60_bss_start, overlay_188b60_bss_end - overlay_188b60_bss_start);
block_73:
    func_00131050();
    var_v0_477 = D_801F365D;
    
    state_value = 0x14;
    *state_base_s1 = state_value;
    goto block_210_flags_or;
block_74:
    var_a0_615 = 0;
    if (((*(s32 *)((s8 *)(current_object) + (0))) & 0xC0) != 0x40) {
        goto block_76;
    }
    var_v0_619 = D_801F365D & 0xFE;
    goto block_224;
block_76:
    class_search_v1 = temp_a1_571;
loop_77:
    if (D_801F1039[var_a0_615] == class_search_v1) {
        goto block_79;
    }
    var_a0_615 += 1;
    if (var_a0_615 < 8) {
        goto loop_77;
    }
block_79:
    if (var_a0_615 == 8) {
        goto block_227;
    }
    goto block_115;
dispatch_case_15:
    var_s2_638 = 0;
    cleanup_mask = -2;
    var_s1_641 = D_801F0CB0;
    do {
    current_object = *var_s1_641;
    object_header_word = *current_object;
    if (object_header_word & 0x10) {
        goto block_84;
    }
    *current_object = object_header_word & cleanup_mask;
    func_00147BDC(current_object);
block_84:
    var_s2_638 += 1;
    var_s1_641 += 1;
    } while (var_s2_638 < 0x32);
    func_0015DF10(0x17C);
    /* Qualify the integer read; the later shifted base accesses separate float objects. */
    temp_s2_734 = (void *)&D_801F0E00;
    temp_a2_661 = *(volatile s32 *)temp_s2_734;
    current_object = D_801F0CB0[temp_a2_661];
    initial_object_flags_v1 = *(s32 *)((s8 *)current_object + 0);
    initial_object_mask_v0 = 0x80000;
    D_801F3658 = temp_a2_661;
    initial_object_flags_v1 |= initial_object_mask_v0;
    *(s32 *)((s8 *)current_object + 0) = initial_object_flags_v1;
    func_0012EA80(-1, &output_position[0]);
    (*(f32 *)((s8 *)(current_object) + (8))) = output_position[0];
    (*(f32 *)((s8 *)(current_object) + (0xC))) = output_position[1];
    (*(f32 *)((s8 *)(current_object) + (0x10))) = output_position[2];
    normalized_z = output_position[2];
    temp_f2_683 = D_801F0D9C;
    normalized_z -= temp_f2_683;
    object_scale = 64.0f;
    normalized_z *= object_scale;
    normalized_z /= D_801F0DA4 - temp_f2_683;
    normalization_work = D_801F0D98;
    normalized_x = output_position[0];
    normalized_x -= normalization_work;
    normalized_x *= object_scale;
    normalized_x /= D_801F0DA0 - normalization_work;
    object_height = 0.75f;
    
    primary_loaded_header = *(s32 *)((s8 *)current_object + 0);
    primary_object_arg = current_object;
    
    /* Distinct results expose the existing mask chain's scheduling priorities.
     * Coalescing these phases early moves the 0.75 load past the header setup;
     * the compiler's later allocation coalesces them into the retail v0. */
    primary_first_masked_header = primary_loaded_header & ~4;
    primary_second_masked_header = primary_first_masked_header & ~0x200;
    primary_object_flags = primary_second_masked_header | 0x21;
    (*(s8 *)((s8 *)(current_object) + (0x20))) = 0;
    (*(f32 *)((s8 *)(current_object) + (0x18))) = object_height;
    (*(f32 *)((s8 *)(current_object) + (0x1C))) = object_height;
    (*(s32 *)((s8 *)(current_object) + (0))) = primary_object_flags;
    (*(s32 *)((s8 *)(current_object) + (0x14))) = ((s32) normalized_z << 6) + (s32) normalized_x;
    func_00129948(primary_object_arg);
    var_s1_641 = D_801F0CB0;
    if (!(*(s32 *)((s8 *)current_object + 0) & 0x40)) {
        goto block_90;
    }
    child_record_index = func_0012E968(current_object);
    record_table_s4 = (void **)var_s1_641;
    temp_s2_734 = (s8 *)temp_s2_734 - 0x64;
    
    child_record_offset = child_record_index * 0xB;
    child_record_byte = D_801969B9[child_record_offset];
    child_record_list = D_801969BA + child_record_offset;
    var_s1_745 = child_record_list + 1;
    child_loop_end = child_record_list + 5;
    child_record_index = child_record_byte | 4;
    D_801969B9[child_record_offset] = child_record_index;
loop_87:
    temp_v1_752 = *var_s1_745;
    if (temp_v1_752 == 0xFF) {
        goto block_89;
    }
    current_object = record_table_s4[temp_v1_752];
    (*(f32 *)((s8 *)(current_object) + (8))) = output_position[0];
    (*(f32 *)((s8 *)(current_object) + (0xC))) = output_position[1];
    (*(f32 *)((s8 *)(current_object) + (0x10))) = output_position[2];
    normalization_work = output_position[2];
    child_z_base = *(f32 *)temp_s2_734;
    normalization_work -= child_z_base;
    normalization_work *= object_scale;
    normalization_work /= (*(f32 *)((s8 *)temp_s2_734 + 8)) - child_z_base;
    child_x_base = (*(f32 *)((s8 *)(temp_s2_734) + (-4)));
    child_normalized_x = output_position[0];
    child_normalized_x -= child_x_base;
    child_normalized_x *= object_scale;
    child_normalized_x /= (*(f32 *)((s8 *)temp_s2_734 + 4)) - child_x_base;
    object_header_word = *(s32 *)((s8 *)current_object + 0);
    
    child_flag_mask = ~4;
    child_object_arg = current_object;
    
    object_header_word &= child_flag_mask;
    
    child_flag_mask = ~0x200;
    object_header_word &= child_flag_mask;
    object_header_word |= 0x21;
    (*(s8 *)((s8 *)(current_object) + (0x20))) = 0;
    (*(f32 *)((s8 *)(current_object) + (0x18))) = object_height;
    (*(f32 *)((s8 *)(current_object) + (0x1C))) = object_height;
    (*(s32 *)((s8 *)(current_object) + (0))) = object_header_word;
    (*(s32 *)((s8 *)(current_object) + (0x14))) = ((s32) normalization_work << 6) + (s32) child_normalized_x;
    func_00129948(child_object_arg);
block_89:
    var_s1_745 += 1;
    if ((s32) var_s1_745 < (s32) child_loop_end) {
        goto loop_87;
    }
block_90:
    (*(s32 *)((s8 *)(current_object) + (0))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0))) | 0x800000);
    (*(f32 *)((s8 *)(current_object) + (0x4C))) = (f32) (*(f32 *)((s8 *)(current_object) + (8)));
    (*(f32 *)((s8 *)(current_object) + (0x50))) = (f32) (*(f32 *)((s8 *)(current_object) + (0xC)));
    (*(f32 *)((s8 *)(current_object) + (0x54))) = (f32) (*(f32 *)((s8 *)(current_object) + (0x10)));
    (*(s32 *)((s8 *)(current_object) + (0x64))) = (s32) (*(s32 *)((s8 *)(current_object) + (0x14)));
    if ((*(s32 *)((s8 *)(current_object) + (0))) & 0x40) {
        goto block_92;
    }
    var_a0_821 = current_object;
    goto block_93;
block_92:
    var_a0_821 = D_801F0CB0[D_801969BA[func_0012E968(current_object) * 0xB]];
block_93:
    func_00109C3C(var_a0_821);
    D_801F0DE0 = 4;
    
    D_801F0FE1 = func_0012F530(current_object);
    D_801F0FE0 = 0;
    D_801F0FDF = 0;
    goto function_return;
dispatch_case_0b:
    current_object = D_801F0CB0[D_801F0E00];
    D_801F36D0 = 0x74;
    /* The actual pointer argument is captured before the word update;
     * all three paths pass the same current object to the shared call. */
    shared_record_call_argument = current_object;
    *(s32 *)((s8 *)current_object + 0x78) &= -2;
    goto block_103;
dispatch_case_2b:
    current_object = D_801F0CB0[D_801F0E00];
    D_801F0DFC = 0;
    (*(s32 *)((s8 *)(current_object) + (0x78))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0x78))) & 0xFFDFFFFF);
    D_8018F500 = func_0012E7E8(current_object);
    D_801F0DE0 = 0x2D;
    D_801F36D0 = 0x16EU;
    D_801F3680 = 0xE;
    D_801F365D = (u8) (D_801F365D | 1);
    goto block_199;
dispatch_case_11:
    current_object = D_801F0CB0[D_801F0E00];
    (*(s32 *)((s8 *)(current_object) + (0x78))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0x78))) & ~0x40);
    D_801F0DFC = 0;
    case11_result_v0 = func_0012E7E8(current_object);
    case11_record_index_a0 = D_801F0E04;
    
    D_8018F500 = case11_result_v0;
    case11_offset_v1 = case11_record_index_a0 * 0x24;
    /* The same actual record byte address is consumed directly by its
     * existing word store; the later loaded-word/message lifetime is separate. */
    D_8018F504 = (s32)(D_801951B0 + case11_offset_v1);
    /* Keep the address phase separate from the loaded flag/message values.
     * This compiler retains a death USE for the eliminated address; sharing
     * its local with the later values removes the corresponding frame home. */
    case11_field_address = (s32)0x80190000U;
    case11_field_address += case11_offset_v1;
    case11_integer_word = *(u16 *)(case11_field_address + 0x51CC);
    case11_integer_word &= 4;
    /* Each path writes its selected message before the argument-read join. */
    if (case11_integer_word != 0) {
        D_801F36D0 = 0x10;
    } else {
        D_801F36D0 = 0x29;
    }
    
    case11_record_index_a0 = D_801F0E00;
    case11_record_index_a1 = D_801F0E04;
    case11_integer_word = func_0012FBB8(case11_record_index_a0, case11_record_index_a1);
    if (case11_integer_word == 0) {
        goto block_100;
    }
    case11_integer_word = 0x29;
    D_801F36D0 = case11_integer_word;
block_100:
    func_00131050();
    func_000ec838(0, (D_801F36D0 + func_801DCF90(current_object)) & 0xFFFF, 0xA0, 0x78, 0xA0, 0x32, (s32) (*(u8 *)((s8 *)((((*(u8 *)((s8 *)((((*(u8 *)((s8 *)(current_object) + (4))) * 0x19) + 0x80190000)) + (0x71F2))) * 0x38) + 0x80190000)) + (0x3BD1))), 0, 0);
    case11_record_index_a0 = ((Func0010DDB4LateIndexWord *)&D_801F0E04)->value;
    D_801F0DE0 = 0x2D;
    temp_v1_985 = case11_record_index_a0 * 0x24;
    (*(s16 *)((s8 *)((temp_v1_985 + 0x80190000)) + (0x51CC))) = (s16) ((*(u16 *)((s8 *)((temp_v1_985 + 0x80190000)) + (0x51CC))) & 0xFFFB);
    case11_index_v1 = D_801F0E04;
    
    case11_offset_v0 = case11_index_v1 * 0x24;
    
    case11_index_v1 = *(u16 *)((s8 *)((case11_offset_v0 + 0x80190000)) + (0x51CC));
    /* Reuse the completed record-index lifetime for the real byte-reset
     * value; the two halfword updates still use their existing working local. */
    case11_record_index_a0 = -1;
    
    D_801F3680 = case11_record_index_a0;
    case11_index_v1 |= 2;
    (*(s16 *)((s8 *)((case11_offset_v0 + 0x80190000)) + (0x51CC))) = (s16) case11_index_v1;
    goto function_return;
dispatch_case_0c:
    current_object = D_801F0CB0[D_801F0E00];
    D_801F36D0 = 0x42U;
    /* The actual pointer argument is captured before the word update;
     * all three paths pass the same current object to the shared call. */
    shared_record_call_argument = current_object;
    *(s32 *)((s8 *)current_object + 0x78) &= -3;
    goto block_103;
dispatch_case_12:
    current_object = D_801F0CB0[D_801F0E00];
    D_801F36D0 = 0x5BU;
    /* The actual pointer argument is captured before the word update;
     * all three paths pass the same current object to the shared call. */
    shared_record_call_argument = current_object;
    *(s32 *)((s8 *)current_object + 0x78) &= -0x81;
block_103:
    D_8018F500 = func_0012E7E8(shared_record_call_argument);
    func_00131050();
    func_000ec838(0, (D_801F36D0 + func_801DCF90(current_object)) & 0xFFFF, 0xA0, 0x78, 0xA0, 0x32, (s32) (*(u8 *)((s8 *)((((*(u8 *)((s8 *)((((*(u8 *)((s8 *)(current_object) + (4))) * 0x19) + 0x80190000)) + (0x71F2))) * 0x38) + 0x80190000)) + (0x3BD1))), 0, 0);
    shared_message_state_v0 = 0x2D;
    
    D_801F0DE0 = shared_message_state_v0;
    shared_message_state_v0 = -1;
    
    D_801F3680 = shared_message_state_v0;
    D_801F0DFC = 0;
    
    goto function_return;
dispatch_case_13:
    case13_index = D_801F0E00;
    D_801F0DE0 = 9;
    /* The existing pointer-table entry is four bytes. This raw byte-address
     * form tests array-access alias metadata without another load or field. */
    current_object = *(void **)((s32)D_801F0CB0 + case13_index * 4);
    D_801F3680 = -1;
    D_801F36D0 = 7;
    D_801F3674 = 2;
    /* Existing two word updates; keep their meanings as raw masks. */
    *(s32 *)((s8 *)current_object + 0x78) &= ~0x100;
    *(s32 *)current_object |= 0x400000;
    D_801F0DFC = 0;
    goto function_return;
dispatch_case_16:
    os_inval_icache(overlay_a_icache_text_start, overlay_a_icache_data_start - overlay_a_icache_text_start);
    os_inval_dcache(overlay_a_dcache_data_start, overlay_a_dcache_bss_start - overlay_a_dcache_data_start);
    func_0002de50(overlay_a_dma_rom_start, overlay_a_dma_text_start, overlay_a_dma_rom_end - overlay_a_dma_rom_start);
    if (overlay_a_bss_start == overlay_a_bss_end) {
        goto block_107;
    }
    memset_00023780(overlay_a_bss_start, overlay_a_bss_end - overlay_a_bss_start);
block_107:
    func_0013109C(*(u16 *)0x801F0E02);
    var_v1_2224 = 0xA;
    D_801F0DE0 = var_v1_2224;
    var_v0_477 = D_801F365D | 1;
    goto block_212;
dispatch_case_17:
    temp_v0_1157 = (D_801F0E00);
    current_object = D_801F0CB0[temp_v0_1157];
    D_801F3658 = temp_v0_1157;
    case17_selector_or_flags = (*(u8 *)((s8 *)(current_object) + (4)));
    case17_index_or_flags = 0;
    if (case17_selector_or_flags < 0x1EU) {
        goto loop_112;
    }
    os_inval_icache(overlay_188b60_icache_text_start, overlay_188b60_icache_data_start - overlay_188b60_icache_text_start);
    os_inval_dcache(overlay_188b60_dcache_data_start, overlay_188b60_dcache_bss_start - overlay_188b60_dcache_data_start);
    func_0002de50(overlay_188b60_dma_rom_start, overlay_188b60_dma_text_start, overlay_188b60_dma_rom_end - overlay_188b60_dma_rom_start);
    if (overlay_188b60_bss_start == overlay_188b60_bss_end) {
        goto block_111;
    }
    memset_00023780(overlay_188b60_bss_start, overlay_188b60_bss_end - overlay_188b60_bss_start);
block_111:
    case17_index_or_flags = D_801F365D;
    state_value = 0x14;
    D_801F0DE0 = state_value;
    case17_index_or_flags |= 1;
    D_801F365D = (u8) case17_index_or_flags;
    
    goto function_return;
loop_112:
    if (D_801F1039[case17_index_or_flags] == case17_selector_or_flags) {
        goto block_114;
    }
    case17_index_or_flags += 1;
    if (case17_index_or_flags < 8) {
        goto loop_112;
    }
block_114:
    if (case17_index_or_flags == 8) {
        goto block_116;
    }
block_115:
    case17_selector_or_flags = D_801F365D;
    case17_state_or_value = 9;
    D_801F0DE0 = case17_state_or_value;
    case17_state_or_value = -1;
    D_801F3680 = case17_state_or_value;
    case17_state_or_value = 0x76;
    D_801F36D0 = case17_state_or_value;
    case17_state_or_value = 1;
    D_801F3674 = case17_state_or_value;
    case17_selector_or_flags |= 1;
    D_801F365D = (u8) case17_selector_or_flags;
    
    func_0015DF10(9);
    goto function_return;
block_116:
    case17_state_or_value = 4;
    D_801F0DE0 = case17_state_or_value;
    
    D_801F0FE1 = func_0012F530(current_object);
    D_801F0FE0 = 0;
    D_801F0FDF = 0;
    var_v1_548 = D_801F365D;
block_117:
    
    D_801F0DEC = 0;
    D_801F0DF0 = 0;
    
block_117_flags:
    var_v1_548 |= 1;
    D_801F365D = var_v1_548;
    goto function_return;
dispatch_case_18:
    func_0015DF10(2);
    func_0011BB4C();
    goto function_return;
dispatch_case_19:
    var_v1_2224 = 1;
    D_801F0DE0 = var_v1_2224;
    var_v0_619 = D_801F365D & 0xFE;
    goto block_224;
dispatch_case_0f:
    case0f_secondary_word = (u32)D_801F0CB0[D_801F0E04];
    if (!((*(s32 *)((s8 *)(case0f_secondary_word) + (0))) & 0x20000)) {
        goto block_123;
    }
    (*(s8 *)((s8 *)(case0f_secondary_word) + (0xB8))) = 0x30;
    (*(s8 *)((s8 *)(case0f_secondary_word) + (0xB9))) = 2;
block_123:
    temp_v1_1299 = (*(s32 *)((s8 *)(case0f_secondary_word) + (0)));
    if (!(temp_v1_1299 & 0x1000)) {
        goto block_125;
    }
    (*(s32 *)((s8 *)(case0f_secondary_word) + (0))) = (s32) (temp_v1_1299 & ~0x1000);
    (*(s32 *)((s8 *)(case0f_secondary_word) + (0xAC))) = 0;
    (*(s32 *)((s8 *)(case0f_secondary_word) + (0xB0))) = 0;
    (*(s8 *)((s8 *)(case0f_secondary_word) + (0xB4))) = 0;
block_125:
    (*(s32 *)((s8 *)(case0f_secondary_word) + (0x78))) = (s32) ((*(s32 *)((s8 *)(case0f_secondary_word) + (0x78))) & ~0x10);
    current_object = D_801F0CB0[D_801F0E00];
    D_8018F500 = func_0012E7E8(current_object);
    if (func_801DD2B0(current_object) != 0) {
        goto block_128;
    }
    if (func_0012DA10(current_object) == 0) {
        goto block_128;
    }
    if (func_0012DA9C(current_object) == 0) {
        goto block_129;
    }
block_128:
    D_801F36D0 = 5U;
    goto block_137;
block_129:
    temp_f0_1329 = func_00118500(current_object, (void *)case0f_secondary_word);
    case0f_secondary_word = 0;
    if ((D_801EE2E0 <= (f64) temp_f0_1329) &&
        ((f64) temp_f0_1329 < D_801EE2E8)) {
        case0f_secondary_word = 2;
        goto block_132;
    }
block_132:
    /* Each selected arm performs the same one halfword message write.
     * No message value stays live across another predicate or the helper. */
    if (*(s32 *)current_object & 0x20000) {
        D_801F36D0 = 0x21D;
    } else if (case0f_secondary_word != 0) {
        D_801F36D0 = 0x236;
    } else {
        case0f_helper_word = func_00022e60();
        case0f_helper_word %= 100;
        if (case0f_helper_word < 0x15) {
            D_801F36D0 = 0x204;
        } else {
            D_801F36D0 = 0x1EB;
        }
    }
    func_00131050();
    func_000ec838(0, (D_801F36D0 + func_801DCF90(current_object)) & 0xFFFF, 0xA0, 0x78, 0xA0, 0x32, (s32) (*(u8 *)((s8 *)((((*(u8 *)((s8 *)((((*(u8 *)((s8 *)(current_object) + (4))) * 0x19) + 0x80190000)) + (0x71F2))) * 0x38) + 0x80190000)) + (0x3BD1))), 0, 0);
    *(s8 *)0x8018F53C = 1;
dispatch_case_2e:
block_137:
    case2e_record_index_v0 = D_801F0E00;
    
    case2e_state_v1 = 0x11;
    
    D_801F0DE0 = case2e_state_v1;
    
    case2e_flags_v1 = D_801F365D;
    case2e_mode_a0 = D_801F0CA4;
    
    /* The same four-byte entry, represented by its actual byte address;
     * inspect whether the state store retains its retail order here. */
    current_object = *(s32 **)((s32)D_801F0CB0 + case2e_record_index_v0 * 4);
    D_801F3680 = -1;
    D_801F365D = (u8) (case2e_flags_v1 | 1);
    if (case2e_mode_a0 != 0x2E) {
        goto block_143;
    }
    D_8018F500 = func_0012E7E8(current_object);
    case2e_index_v1 = D_801F0E04;
    case2e_index_v1 -= 0x100;
    
    D_8018F504 = (s32) (*(s32 *)((s8 *)(((case2e_index_v1 * 0x48) + 0x80180000)) + (0x7C14)));
    if (*(u8 *)0x800E9C16 != 2) {
        goto block_142;
    }
    if (func_0012DA10(current_object) == 0) {
        goto block_142;
    }
    if (func_0012DA9C(current_object) != 0) {
        goto block_142;
    }
    D_801F36D0 = 0x1B9U;
    func_00131050();
    func_000ec838(0, (D_801F36D0 + func_801DCF90(current_object)) & 0xFFFF, 0xA0, 0x78, 0xA0, 0x32, (s32) (*(u8 *)((s8 *)((((*(u8 *)((s8 *)((((*(u8 *)((s8 *)(current_object) + (4))) * 0x19) + 0x80190000)) + (0x71F2))) * 0x38) + 0x80190000)) + (0x3BD1))), 0, 0);
    goto block_146;
block_142:
    D_801F36D0 = 0x32U;
    goto block_146;
block_143:
    if (func_801DD2B0(current_object) != 0) {
        goto block_146;
    }
    if (func_0012DA10(current_object) == 0) {
        goto block_146;
    }
    func_0012DA9C(current_object);
block_146:
    os_inval_icache(overlay_17f9c0_icache_text_start, overlay_17f9c0_icache_data_start - overlay_17f9c0_icache_text_start);
    os_inval_dcache(overlay_17f9c0_dcache_data_start, overlay_17f9c0_dcache_bss_start - overlay_17f9c0_dcache_data_start);
    func_0002de50(overlay_17f9c0_dma_rom_start, overlay_17f9c0_dma_text_start, overlay_17f9c0_dma_rom_end - overlay_17f9c0_dma_rom_start);
    if (overlay_17f9c0_bss_start == overlay_17f9c0_bss_end) {
        goto block_148;
    }
    memset_00023780(overlay_17f9c0_bss_start, overlay_17f9c0_bss_end - overlay_17f9c0_bss_start);
block_148:
    if (!((*(s32 *)((s8 *)(current_object) + (0))) & 0x20000)) {
        goto block_150;
    }
    (*(s8 *)((s8 *)(current_object) + (0xB8))) = 0x30;
    (*(s8 *)((s8 *)(current_object) + (0xB9))) = 2;
block_150:
    temp_v1_1563 = (*(s32 *)((s8 *)(current_object) + (0)));
    if (!(temp_v1_1563 & 0x1000)) {
        goto block_152;
    }
    (*(s32 *)((s8 *)(current_object) + (0))) = (s32) (temp_v1_1563 & ~0x1000);
    (*(s32 *)((s8 *)(current_object) + (0xAC))) = 0;
    (*(s32 *)((s8 *)(current_object) + (0xB0))) = 0;
    (*(s8 *)((s8 *)(current_object) + (0xB4))) = 0;
block_152:
    *(s32 *)((s8 *)current_object + 0x7C) = -1;
    *(s32 *)((s8 *)current_object + 0x78) &= ~0x10;
    D_801F0DFC = 0;
    goto function_return;
dispatch_case_1a:
    current_object = D_801F0CB0[D_801F0E00];
    (*(s32 *)((s8 *)(current_object) + (0x78))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0x78))) & ~0x400);
    D_801F0DFC = 0;
    D_8018F500 = func_0012E7E8(current_object);
    D_801F0DE0 = 0x2D;
    
    D_801F36D0 = 0x8D;
    goto shared_timer_stored;
dispatch_case_1b:
    current_object = D_801F0CB0[D_801F0E00];
    (*(s32 *)((s8 *)(current_object) + (0x78))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0x78))) & ~0x800);
    D_801F0DFC = 0;
    D_8018F500 = func_0012E7E8(current_object);
    D_801F0DE0 = 0x2D;
    
    D_801F36D0 = 0xA6;
    goto shared_timer_stored;
dispatch_case_1e:
    current_object = D_801F0CB0[D_801F0E00];
    (*(s32 *)((s8 *)(current_object) + (0x78))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0x78))) & ~0x2000);
    D_801F0DFC = 0;
    func_00131050();
    temp_v1_1641 = (*(s32 *)((s8 *)(current_object) + (0)));
    
    case1e_table_s1 = D_801F0CB0;
    
    var_a0_1646 = current_object;
    if (temp_v1_1641 & 0x40) {
        goto block_157;
    }
    (*(s32 *)((s8 *)(current_object) + (0))) = (s32) (temp_v1_1641 | 0x20000);
    goto block_162;
block_157:
    case1e_record_index = func_0012E968(current_object);
    case1e_v1 = case1e_record_index * 0xB;
    
    case1e_v0 = (s32)D_801969B8;
    
    case1e_list_a1 = (u8 *)(case1e_v1 + case1e_v0);
    case1e_loop_a0 = 0;
    case1e_ff_t0 = 0xFF;
    case1e_table_a3 = case1e_table_s1;
    case1e_mask_a2 = 0x20000;
    
loop_158:
    /* One real address computation per iteration, including entry and
     * backedges; do not peel the zero-counter first iteration in source. */
    case1e_v0 = (s32)(case1e_list_a1 + case1e_loop_a0);
    case1e_v0 = *(u8 *)(case1e_v0 + 2);
    case1e_loop_a0 += 1;
    if (case1e_v0 == case1e_ff_t0) {
        goto block_160;
    }
    case1e_v1 = (s32)case1e_table_a3[case1e_v0];
    case1e_v0 = *(s32 *)case1e_v1;
    case1e_v0 |= case1e_mask_a2;
    *(s32 *)case1e_v1 = case1e_v0;
block_160:
    if (case1e_loop_a0 < 5) {
        goto loop_158;
    }
    var_a0_1646 = D_801F0CB0[*(u8 *)(case1e_list_a1 + 2)];
block_162:
    D_8018F500 = func_0012E7E8(var_a0_1646);
    if (func_0012DA10(current_object) == 0) {
        goto block_167;
    }
    if (func_0012DA9C(current_object) != 0) {
        goto block_167;
    }
    if (!((*(s32 *)((s8 *)(current_object) + (0))) & 0x40)) {
        goto block_166;
    }
    current_object = D_801F0CB0[D_801969BA[func_0012E968(current_object) * 0xB]];
block_166:
    D_801F0DE0 = 0x2D;
    D_801F3680 = -1;
    D_801F36D0 = 0xBFU;
    goto block_199;
block_167:
    D_801F0DE0 = 9;
    var_v0_1730 = 0x11;
    goto block_192;
dispatch_case_1f:
    case1f_index_v0 = D_801F0E00;
    
    /* Actual existing pointer-table byte offset; end the unscaled value
     * lifetime before loading the object, then reuse it for the flag. */
    case1f_index_v0 <<= 2;
    current_object = *(s32 **)((s32)D_801F0CB0 + case1f_index_v0);
    
    /* The first index's lifetime ends at the lookup. Reuse that real
     * working value for the flag update before the second index read. */
    case1f_index_v0 = *(s32 *)((s8 *)current_object + 0x78);
    case1f_mask_work = ~0x4000;
    case1f_index_v0 &= case1f_mask_work;
    *(s32 *)((s8 *)current_object + 0x78) = case1f_index_v0;
    case1f_index_v0 = D_801F0E00;
    
    D_801F0DFC = 0;
    
    D_8018F500 = func_0012E7E8(*(s32 **)(0x801F0CB0 + (case1f_index_v0 * 4)));
    func_00131050();
    /* Reuse the finished mask lifetime for the existing header clear. */
    case1f_mask_work = ~0x20000;
    *(s32 *)current_object &= case1f_mask_work;
    if (func_0012DA10(current_object) == 0) {
        goto block_171;
    }
    if (func_0012DA9C(current_object) != 0) {
        goto block_171;
    }
    D_801F0DE0 = 0x2D;
    D_801F3680 = -1;
    D_801F36D0 = 0xD8U;
    goto block_199;
block_171:
    D_801F0DE0 = 9;
    var_v0_1730 = 0x12;
    goto block_192;
dispatch_case_21:
    current_object = D_801F0CB0[D_801F0E00];
    D_801F0DFC = 0;
    D_8018F500 = func_0012E7E8(current_object);
    D_801F3658 = (s32) (D_801F0E00);
    var_v0_1810 = func_0012DA10(current_object);
    if (var_v0_1810 == 0) {
        var_v0_1818 = 9;
        goto block_176;
    }
block_174:
    var_v0_1810 = 0x2D;
    D_801F0DE0 = var_v0_1810;
    var_v0_1810 = 0x10A;
    D_801F36D0 = var_v0_1810;
    var_v0_1810 = 5;
    goto block_198;
block_176:
    D_801F0DE0 = var_v0_1818;
    var_v0_1818 = 0x1A;
    D_801F36D0 = var_v0_1818;
    var_v0_1818 = 5;
    goto block_193;
dispatch_case_20:
    D_801F0DE0 = 0x15;
    
    goto function_return;
dispatch_case_22:
    case22_index_v0 = D_801F0E00;
    
    current_object = D_801F0CB0[case22_index_v0];
    
    case22_mask_v1 = 0xFFFF0000;
    
    case22_flags_v0 = (*(s32 *)((s8 *)(current_object) + (0x78)));
    case22_mask_v1 |= 0x7FFF;
    case22_table_s1 = D_801F0CB0;
    case22_flags_v0 &= case22_mask_v1;
    (*(s32 *)((s8 *)(current_object) + (0x78))) = case22_flags_v0;
    case22_state_v0 = 0x2D;
    D_801F0DFC = 0;
    
    D_801F0DE0 = case22_state_v0;
    case22_object_flags_v1 = (*(s32 *)((s8 *)(current_object) + (0)));
    case22_minus_one_v0 = -1;
    
    D_801F3680 = case22_minus_one_v0;
    
    case22_object_flags_v1 &= 0x80;
    if (!case22_object_flags_v1) {
        goto block_180;
    }
    D_801F36D0 = 0xF1;
    D_8018F500 = func_0012E7E8(case22_table_s1[D_801969BA[func_0012E968(current_object) * 0xB]]);
    var_s1_1877 = (*(u8 *)((s8 *)(((func_0012E968(current_object) * 0xB) + 0x80190000)) + (0x69BA)));
    goto block_181;
block_180:
    D_801F36D0 = 0x155;
    D_8018F500 = func_0012E7E8(current_object);
    var_s1_1877 = (*(u8 *)((s8 *)(current_object) + (4)));
block_181:
    func_00131050();
    func_000ec838(0, (D_801F36D0 + func_801DCF90(current_object)) & 0xFFFF, 0xA0, 0x78, 0xA0, 0x32, (s32) (*(u8 *)((s8 *)((((*(u8 *)((s8 *)(((var_s1_1877 * 0x19) + 0x80190000)) + (0x71F2))) * 0x38) + 0x80190000)) + (0x3BD1))), 0, 0);
    func_0012F140(current_object);
    goto function_return;
dispatch_case_23:
    case23_index_v0 = D_801F0E00;
    case23_mask_a1 = 0xFFFDFFFF;
    
    current_object = D_801F0CB0[case23_index_v0];
    
    case23_loaded_flag_byte = D_801F365D;
    case23_flags_a0 = (*(s32 *)((s8 *)(current_object) + (0x78)));
    case23_minus_one_v0 = -1;
    
    /* Separate the actual loaded byte from the masked working result.
     * Both feed the same original byte store; no additional read. */
    case23_global_v1 = case23_loaded_flag_byte & 0xFE;
    D_801F3658 = case23_minus_one_v0;
    D_801F365D = case23_global_v1;
    
    case23_flags_a0 &= case23_mask_a1;
    (*(s32 *)((s8 *)(current_object) + (0x78))) = case23_flags_a0;
    D_801F0DFC = 0;
    D_801F0DE0 = 0;
    
    var_s5_11 = 0x23;
    goto block_226;
dispatch_case_25:
    current_object = D_801F0CB0[D_801F0E00];
    
    case25_mask_v1 = 0xFFFB0000;
    
    case25_flags_v0 = (*(s32 *)((s8 *)(current_object) + (0x78)));
    case25_mask_v1 |= 0xFFFF;
    
    case25_flags_v0 &= case25_mask_v1;
    (*(s32 *)((s8 *)(current_object) + (0x78))) = case25_flags_v0;
    D_801F0DFC = 0;
    D_8018F500 = func_0012E7E8(current_object);
    D_801F0DE0 = 0x2D;
    D_801F3680 = -1;
    var_v0_1977 = func_0012DA9C(current_object);
    if (var_v0_1977 != 0) {
        goto case25_nonzero;
    }
    var_v0_1977 = 0x123;
    goto block_185;
case25_nonzero:
    var_v0_1977 = 0x187;
block_185:
    D_801F36D0 = var_v0_1977;
    func_00131050();
    func_000ec838(0, (D_801F36D0 + func_801DCF90(current_object)) & 0xFFFF, 0xA0, 0x78, 0xA0, 0x32, (s32) (*(u8 *)((s8 *)((((*(u8 *)((s8 *)((((*(u8 *)((s8 *)(current_object) + (4))) * 0x19) + 0x80190000)) + (0x71F2))) * 0x38) + 0x80190000)) + (0x3BD1))), 0, 0);
    func_0012F8C0(current_object);
    goto function_return;
dispatch_case_26:
    current_object = D_801F0CB0[D_801F0E00];
    (*(s32 *)((s8 *)(current_object) + (0x78))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0x78))) & 0xFFF7FFFF);
    D_801F0DFC = 0;
    D_8018F500 = func_0012E7E8(current_object);
    if (func_0012DA10(current_object) == 0) {
        goto block_191;
    }
    if (func_0012DA9C(current_object) == 0) {
        goto block_189;
    }
    goto block_191;
block_189:
    D_801F0DE0 = 0x2D;
    D_801F36D0 = 0x13C;
    goto shared_timer_stored;
block_191:
    D_801F0DE0 = 9;
    var_v0_1730 = 0x21;
block_192:
    D_801F36D0 = var_v0_1730;
    var_v0_1818 = -1;
block_193:
    D_801F3680 = var_v0_1818;
    var_v0_1818 = 2;
    D_801F3674 = var_v0_1818;
    goto function_return;
dispatch_case_32:
    current_object = D_801F0CB0[D_801F0E00];
    (*(s32 *)((s8 *)(current_object) + (0x78))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0x78))) & 0xFDFFFFFF);
    D_801F0DFC = 0;
    D_8018F500 = func_0012E7E8(current_object);
    if (func_0012DA10(current_object) == 0) {
        goto block_200;
    }
    if (func_0012DA9C(current_object) != 0) {
        goto block_200;
    }
    D_801F0DE0 = 0x2D;
    D_801F36D0 = 0x24F;
shared_timer_stored:
    var_v0_1810 = -1;
block_198:
    D_801F3680 = var_v0_1810;
block_199:
    func_00131050();
    func_000ec838(0, (D_801F36D0 + func_801DCF90(current_object)) & 0xFFFF, 0xA0, 0x78, 0xA0, 0x32, (s32) (*(u8 *)((s8 *)((((*(u8 *)((s8 *)((((*(u8 *)((s8 *)(current_object) + (4))) * 0x19) + 0x80190000)) + (0x71F2))) * 0x38) + 0x80190000)) + (0x3BD1))), 0, 0);
    goto function_return;
block_200:
    D_801F0DE0 = 9;
    case32_flags_v1 = (*(s32 *)((s8 *)(current_object) + (0)));
    
    D_801F36D0 = 0x79U;
    D_801F3680 = -1;
    D_801F3674 = 2;
    case32_mask_v0 = 0x02000000;
    case32_flags_v1 |= case32_mask_v0;
    (*(s32 *)((s8 *)(current_object) + (0))) = case32_flags_v1;
    goto block_227;
dispatch_case_27:
    func_0015DF10(0x17C);
    case27_index_v0 = D_801F0E00;
    case27_mask_a1 = 0xFFEF0000;
    
    current_object = D_801F0CB0[case27_index_v0];
    
    case27_mask_a1 |= 0xFFFF;
    /* The existing byte read follows pure local mask completion; keep the
     * actual byte working value for its existing in-place AND and store. */
    case27_global_v0 = D_801F365D;
    case27_flags_v1 = (*(s32 *)((s8 *)(current_object) + (0x78)));
    
    case27_global_v0 &= 0xFE;
    D_801F365D = case27_global_v0;
    
    case27_flags_v1 &= case27_mask_a1;
    (*(s32 *)((s8 *)(current_object) + (0x78))) = case27_flags_v1;
    D_801F0DFC = 0;
    D_801F0DE0 = 0;
    func_0010746C(current_object);
    (*(s32 *)((s8 *)(current_object) + (0))) = (s32) ((*(s32 *)((s8 *)(current_object) + (0))) & ~0x20);
    goto block_227;
dispatch_case_28:
    D_801F0DE0 = 0x1A;
    goto function_return;
dispatch_case_29:
    os_inval_icache(overlay_b_icache_text_start, overlay_b_icache_data_start - overlay_b_icache_text_start);
    os_inval_dcache(overlay_b_dcache_data_start, overlay_b_dcache_bss_start - overlay_b_dcache_data_start);
    func_0002de50(overlay_b_dma_rom_start, overlay_b_dma_text_start, overlay_b_dma_rom_end - overlay_b_dma_rom_start);
    if (overlay_b_bss_start == overlay_b_bss_end) {
        goto block_205;
    }
    memset_00023780(overlay_b_bss_start, overlay_b_bss_end - overlay_b_bss_start);
block_205:
    var_v0_477 = D_801F365D;
    var_v1_2224 = 0x1C;
    goto block_210;
dispatch_case_2a: {
    register s32 case2a_mode_v1;
    register s32 case2a_expected_v0;
    register void *case2a_arg_a0;
    register u8 *case2a_flags_a2;
    register s32 case2a_flags_v1;
    register s32 case2a_call_a1;

    case2a_mode_v1 = D_8018F481;
    case2a_expected_v0 = 0xD;
    
    if (case2a_mode_v1 == case2a_expected_v0) {
        func_0005c090(0xE, 1);
    } else {
        func_0005c090(0xF, 1);
    }
    case2a_flags_a2 = D_8018F520;
    case2a_flags_v1 = *case2a_flags_a2;
    case2a_arg_a0 = (void *)g_func_0015DF10_match;
    case2a_call_a1 = 0x28A;
    
    *(s8 *)0x8018F56C = 1;
    *(s8 *)0x8018F56D = 1;
    *(s32 *)0x8018F568 = 0;
    D_801F0DE0 = 0x1D;
    
    case2a_flags_v1 |= 6;
    *case2a_flags_a2 = case2a_flags_v1;
    func_800EA604(case2a_arg_a0, case2a_call_a1);
    goto function_return;
}
dispatch_case_2c:
    var_v0_477 = D_801F365D;
    var_v1_2224 = 0x1F;
block_210:
    D_801F0DE0 = var_v1_2224;
    var_v1_2224 = -1;
    D_801F0DE8 = var_v1_2224;
    
block_210_flags_or:
    var_v0_477 |= 1;
block_212:
    D_801F365D = var_v0_477;
    
    goto function_return;
dispatch_case_2d:
    final_record_index_v0 = D_801F0E00;
    
    final_v1 = D_801F365D;
    
    final_a0 = D_800E9C16;
    
    /* Keep the complete slot address separate from its scaled index and the
     * loaded object. Its eliminated address retains a death USE and frame home;
     * collapsing the source phases shrinks the retail frame by eight bytes. */
    case2d_slot_address = (s32)D_801F0CB0;
    final_record_index_v0 = (u32)final_record_index_v0 << 2;
    case2d_slot_address += final_record_index_v0;
    current_object = *(s32 **)case2d_slot_address;
    state_base_s1 = &D_801F0DE0;
    final_record_index_v0 = 0x1E;
    *state_base_s1 = final_record_index_v0;
    final_record_index_v0 = -1;
    final_v1 |= 1;
    D_801F0DE8 = final_record_index_v0;
    D_801F365D = (u8) final_v1;
    final_record_index_v0 = 2;
    D_801F0DFC = 0;
    if (final_a0 != final_record_index_v0) {
        goto block_218;
    }
    if (func_0012DA10(current_object) == 0) {
        goto block_218;
    }
    if (func_801DD2B0(current_object) != 0) {
        goto block_218;
    }
    if (func_0012DA9C(current_object) != 0) {
        goto block_218;
    }
    (D_801F0E00) = (s32) (*(u8 *)((s8 *)(current_object) + (4)));
    final_record = func_0012E7E8(current_object);
    final_v1 = D_801F0E04;
    
    final_a0 = D_801F365D;
    
    ((Func0010DDB4ExistingPointerWord *)&D_8018F500)->value = final_record;
    
    /* Unsigned inverse-offset subtraction preserves the 32-bit address.
     * Both CSE passes retain MINUS; combine then cancels the negation into
     * base-first PLUS. A direct addition reverses the retail ADDU operands. */
    final_v1 = (u32)final_v1 << 1;
    final_v1 = 0U - (u32)final_v1;
    final_v1 = (u32)state_base_s1 - (u32)final_v1;
    final_v1 = ((u16 *)final_v1)[0x102];
    D_801F36D0 = 0x1A0U;
    
    final_a0 |= 4;
    D_801F365D = (u8) final_a0;
    D_801F36A2 = (u16) final_v1;
    goto block_219;
block_218:
    func_00132B80(D_801F0FE4[D_801F0E04]);
block_219:
    func_00130E60(D_801F0FE4[D_801F0E04], 0);
    final_a0 = D_801F1024[D_801F0E04];
    var_a1_2356 = final_a0;
    if (final_a0 >= 0) {
        goto block_221;
    }
    var_a1_2356 = final_a0 + 7;
block_221:
    var_a1_2356 >>= 3;
    final_v1 = var_a1_2356 * 8;
    final_v1 = final_a0 - final_v1;
    final_a0 = D_80196A38[var_a1_2356];
    final_result_v0 = 1;
    final_result_v0 <<= final_v1;
    final_a0 |= final_result_v0;
    D_80196A38[var_a1_2356] = (u8) final_a0;
    goto function_return;
dispatch_case_30:
    D_801F0DE0 = 0x2C;
    D_801F3658 = -1;
    D_801F0DE8 = 0;
    D_801F0DFC = 0;
    var_v0_619 = D_801F365D & 0xFE;
    goto block_224;
dispatch_case_31:
    D_801F0DE0 = 0;
    D_801F0DE8 = 0;
    var_v0_619 = D_801F365D & 0xFE;
block_224:
    D_801F365D = var_v0_619;
    goto function_return;
dispatch_case_33:
    D_801F0DE0 = 0x31;
block_226:
    D_801F0DE8 = 0;
dispatch_default:
block_227:
    function_return:
    return var_s5_11;
}

