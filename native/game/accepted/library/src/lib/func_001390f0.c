/* Private complete-body PURE_C control. Both switch matrices follow retail
 * table entries; the exact hybrid remains the reference/fallback. */
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
M2C_UNK func_0002de50(void *, void *, s32);
M2C_UNK func_0005c090(M2C_UNK, M2C_UNK);
s32 func_0005c110(M2C_UNK);
f32 func_000F315C(u8, f32, f32);
M2C_UNK func_001072B8(s32);
M2C_UNK func_0010D484(f32, f32, f32, s32, s32);
M2C_UNK func_00121DA8(void *);
M2C_UNK func_00127154(M2C_UNK, M2C_UNK, M2C_UNK);
s32 func_00127BFC();
u8 func_00127C50();
M2C_UNK func_0012EA80(s32, f32 *);
s32 func_00130C2C();
M2C_UNK func_00130E60(u16, M2C_UNK);
s32 func_00139C10();
s32 func_00139C88();
s32 func_00139CE8();
s32 func_00139D74();
M2C_UNK func_00139DC0(u32, f32 *, u16 *, s8 *);
M2C_UNK func_800EA6B0(void *, M2C_UNK, M2C_UNK, M2C_UNK);
M2C_UNK func_800EA6C8(void *);
M2C_UNK func_802173B0(M2C_UNK);
M2C_UNK memset_00023780(void *, s32);
M2C_UNK os_inval_dcache(void *, s32);
M2C_UNK os_inval_icache(void *, s32);
extern u8 overlay_a_rom_start[];
extern u8 overlay_a_rom_end[];
extern u8 overlay_a_text_start[];
extern u8 overlay_a_data_start[];
extern u8 overlay_a_bss_start[];
extern u8 overlay_a_bss_end[];
extern u8 overlay_a_icache_text_start[];
extern u8 overlay_a_icache_data_start[];
extern u8 overlay_a_dcache_data_start[];
extern u8 overlay_a_dcache_bss_start[];
extern u8 overlay_a_dma_rom_start[];
extern u8 overlay_a_dma_rom_end[];
extern u8 overlay_a_dma_text_start[];
extern u8 overlay_b_rom_start[];
extern u8 overlay_b_rom_end[];
extern u8 overlay_b_text_start[];
extern u8 overlay_b_data_start[];
extern u8 overlay_b_bss_start[];
extern u8 overlay_b_bss_end[];
extern u8 overlay_b_icache_text_start[];
extern u8 overlay_b_icache_data_start[];
extern u8 overlay_b_dcache_data_start[];
extern u8 overlay_b_dcache_bss_start[];
extern u8 overlay_b_dma_rom_start[];
extern u8 overlay_b_dma_rom_end[];
extern u8 overlay_b_dma_text_start[];
extern u8 overlay_c_rom_start[];
extern u8 overlay_c_rom_end[];
extern u8 overlay_c_text_start[];
extern u8 overlay_c_data_start[];
extern u8 overlay_c_bss_start[];
extern u8 overlay_c_bss_end[];
extern u8 overlay_c_icache_text_start[];
extern u8 overlay_c_icache_data_start[];
extern u8 overlay_c_dcache_data_start[];
extern u8 overlay_c_dcache_bss_start[];
extern u8 overlay_c_dma_rom_start[];
extern u8 overlay_c_dma_rom_end[];
extern u8 overlay_c_dma_text_start[];
extern void *scenario_state_slot;
extern s32 D_801F0CB0[];
extern u8 D_800EB0DC[];
extern u8 D_8018F481;
extern volatile s32 D_8018F58C;
extern u8 D_80190F80[];
extern u32 D_80196A6C;
extern f32 D_801F0D80;
extern u8 D_801F0DE0[];
extern s32 D_801F0DE8;
extern u8 D_801F365D;
extern f32 D_801F369C;
extern s32 D_8021C744;


s32 func_001390F0(void) {
    f32 position[3];
    f32 event_position[3];
    register f32 denominator;
    register f32 var_f0_149;
    register f32 var_f20_negative;
    register f32 var_f22_positive;
    register f32 var_f4_140;
    register f32 x_base;
    register f32 y_base;
    f32 zero_float;
    s32 temp_v0_541;
    s32 temp_v1_268;
    s32 temp_v1_548;
    register s32 var_a0_477;
    register u32 s0_value;
    s32 var_v0_333;
    s32 var_v0_336;
    s32 eligibility;
    register s32 signed_invalid;
    register s8 *var_a3_248;
    register s32 temp_v0_250;
    register u16 *var_a2_247;
    register s32 temp_a0_266;
    u16 temp_v1_588;
    u32 temp_v0_695;
    u32 var_s1_236;
    u32 *money;
    register u8 *reward_pair;
    register u8 *var_t0_245;
    register s32 temp_a0_718;
    s32 temp_a1_716;
    u8 temp_v0_302;
    u32 event_dispatch_index;
    s32 temp_v0_83;
    u8 temp_v1_28;
    u8 temp_v1_402;
    u8 temp_v1_508;
    u8 state_flags;
    void *temp_a0_43;
    void *event_owner;
    register u8 *state_base;
    register u16 invalid_timer;

    if (*(s32 *)0x801F3610 == 0) {
        goto block_2;
    }
    if (*(s32 *)0x801F0DE0 != 0) {
        goto block_64;
    }
block_2:
    
    if (*(u8 *)0x801F365D & 2) {
        goto block_64;
    }
    temp_v1_28 = *(u8 *)0x8018F481;
    if (temp_v1_28 != 2) {
        goto block_18;
    }
    if (func_0005c110(0) == 0) {
        goto block_30;
    }
    if (((*(s32 *)D_801F0CB0[30]) & 0x11) != 0x10) {
        goto block_30;
    }
    s0_value = (u32)&D_801F0CB0[30];
    temp_a0_43 = *(void **)s0_value;
    (*(s8 *)((s8 *)(temp_a0_43) + (0xBA))) = 1;
    func_00121DA8(temp_a0_43);
    os_inval_icache(overlay_a_icache_text_start, overlay_a_icache_data_start - overlay_a_icache_text_start);
    os_inval_dcache(overlay_a_dcache_data_start, overlay_a_dcache_bss_start - overlay_a_dcache_data_start);
    func_0002de50(overlay_a_dma_rom_start, overlay_a_dma_text_start, overlay_a_dma_rom_end - overlay_a_dma_rom_start);
    if (overlay_a_bss_start == overlay_a_bss_end) {
        goto block_8;
    }
    memset_00023780(overlay_a_bss_start, overlay_a_bss_end - overlay_a_bss_start);
block_8:
    func_802173B0(0);
    if (!(*(u16 *)0x801952C8 & 2)) {
        return 1;
    }
    temp_v0_83 = *(u8 *)0x801952C1;
    if (temp_v0_83 == 0) {
        return 1;
    }
    for (var_s1_236 = 0; (s32)var_s1_236 < (s32)*(u8 *)0x801952C1; var_s1_236++) {
        u8 *member = (u8 *)*(s32 *)0x801952C4 + var_s1_236;
        s32 member_id = *member;
        if (member_id >= 0) {
            func_001072B8(D_801F0CB0[member_id]);
            func_00127154(0x1E, member_id, 0);
        }
    }
    return 1;
block_18:
    if (temp_v1_28 != 0xD) {
        goto block_23;
    }
    if (func_0005c110(0x11) != 0) {
        goto block_23;
    }
    if (func_0005c110(0xE) != 0) {
        goto block_30;
    }
    if (!(*(u16 *)0x801F0EBA & 8)) {
        goto block_30;
    }
    var_f4_140 = *(f32 *)0x801F0DA0;
    x_base = *(f32 *)0x801F0D98;
    var_f4_140 -= x_base;
    var_f4_140 *= 217.0f;
    denominator = 256.0f;
    var_f4_140 /= denominator;
    
    y_base = *(f32 *)0x801F0D9C;
    var_f0_149 = (*(f32 *)0x801F0DA4 - y_base) * 153.0f / denominator;
    x_base += var_f4_140;
    y_base += var_f0_149;
    goto block_29;
block_23:
    if (*(u8 *)0x8018F481 != 0x11) {
        goto block_30;
    }
    if (func_0005c110(0x11) != 0) {
        goto block_30;
    }
    if (func_0005c110(0xF) != 0) {
        goto block_30;
    }
    if (!(*(u16 *)0x801951CC & 2)) {
        goto block_30;
    }
    if (*(s32 *)0x801F0DFC != 0) {
        goto block_30;
    }
    var_f4_140 = *(f32 *)0x801F0DA0;
    x_base = *(f32 *)0x801F0D98;
    var_f4_140 -= x_base;
    var_f4_140 *= 145.0f;
    denominator = 256.0f;
    var_f4_140 /= denominator;
    
    y_base = *(f32 *)0x801F0D9C;
    var_f0_149 = (*(f32 *)0x801F0DA4 - y_base) * 32.0f / denominator;
    x_base += var_f4_140;
    y_base += var_f0_149;
block_29:
    position[0] = x_base;
    position[2] = y_base;
    position[1] = func_000F315C(*(u8 *)0x800E7AB9, x_base, y_base);
    func_0010D484(position[0], position[1], position[2], D_8018F58C, 0x2A);
    D_801F365D = (u8)(D_801F365D | 1);
    return 1;
block_30:
    if (*(void **)0x801F3610 != 0) {
        goto block_64;
    }
    state_base = D_801F0DE0;
    if (*(s32 *)state_base != 0) {
        goto block_64;
    }
    if (*(void **)0x801F0DFC != 0) {
        goto block_64;
    }
    var_s1_236 = 0;
    signed_invalid = -1;
    invalid_timer = 0xFFFF;
    var_f20_negative = -0.8f;
    var_f22_positive = 0.8f;
    var_t0_245 = state_base + 0x272;
    s0_value = (u32)(state_base + 0x262);
    var_a2_247 = (u16 *)s0_value;
    var_a3_248 = (s8 *)(state_base + 0x259);
loop_34:
    temp_v0_250 = *var_a3_248;
    if (temp_v0_250 == signed_invalid) {
        goto block_63;
    }
    temp_a0_266 = *(s32 *)(0x801F0CB0 + (temp_v0_250 * 4));
    state_base = *(u8 **)0x801F361C;
    state_base += *(s32 *)((s8 *)temp_a0_266 + 0x14);
    temp_v0_250 = *state_base;
    temp_a0_266 = *var_t0_245;
    if (temp_a0_266 != temp_v0_250) {
        goto block_62;
    }
block_37:
    temp_a0_266 = *var_a2_247;
    temp_v1_268 = (*(u8 *)0x800E9C13 + 1) * 2;
    *var_a2_247 = ((s32) temp_a0_266 < temp_v1_268)
        ? 0
        : (temp_v0_250 = temp_a0_266 - temp_v1_268);
block_40:
block_42:
    if (*var_a2_247 != 0) {
        goto block_63;
    }
    zero_float = 0.0f;
    D_801F369C = zero_float;
    D_801F0D80 = zero_float;
    *var_a3_248 = signed_invalid;
    *var_a2_247 = invalid_timer;
    *var_t0_245 = 0;
    func_00139DC0(var_s1_236, event_position, var_a2_247, var_a3_248);
    func_0010D484(event_position[0], event_position[1], event_position[2], D_8018F58C, 0x2C);
    event_owner = *(void **)0x801AB850;
    (*(u32 *)((s8 *)s0_value + (-0x23E))) = var_s1_236;
    (*(s8 *)((s8 *)event_owner + 4)) = 0;
    
    temp_v0_302 = *(u8 *)0x8018F481;
    event_dispatch_index = temp_v0_302 - 0x27;
    switch (event_dispatch_index) {
    case 0: goto dispatch_event_27;
    case 8: case 9: case 21: case 22: goto dispatch_event_group;
    case 12: goto dispatch_event_33;
    default: goto block_128;
    }
dispatch_event_27:
    *(s32 *)0x801F0E00 = var_s1_236 + 1;
    goto block_61;
dispatch_event_group:
    {
    s32 dispatch_word;
    
    switch (var_s1_236) {
    case 0: goto dispatch_index_0;
    case 1: goto dispatch_index_1;
    case 2: goto dispatch_index_2;
    case 3: goto dispatch_index_3;
    case 4: goto dispatch_index_4;
    case 5: goto dispatch_index_5;
    case 6: goto dispatch_index_6;
    default: goto block_128;
    }
dispatch_index_0:
    *(void **)0x801F0E00 = 4;
    goto block_61;
dispatch_index_1:
    dispatch_word = 5;
    goto block_53;
dispatch_index_2:
    dispatch_word = 7;
    goto block_55;
dispatch_index_3:
    *(void **)0x801F0E00 = 6;
    goto block_61;
dispatch_index_4:
    dispatch_word = 8;
block_53:
    *(void **)0x801F0E00 = dispatch_word;
    *(f32 *)0x801F0D80 = var_f20_negative;
    goto block_128;
dispatch_index_5:
    dispatch_word = 0xA;
block_55:
    *(void **)0x801F0E00 = dispatch_word;
    *(f32 *)0x801F0D80 = var_f22_positive;
    goto block_128;
dispatch_index_6:
    *(void **)0x801F0E00 = 9;
    goto block_61;
    }
dispatch_event_33: {
    /* Define the shared result before either test to preserve the retail
     * return-value sets on the two half-unit paths. */
    s32 result;
    *(s32 *)0x801F0E00 = var_s1_236 + 0xB;
    result = 1;
    if (var_s1_236 == 0) {
        *(f32 *)0x801F0D80 = -0.5f;
        return result;
    }
    if (var_s1_236 == result) {
        *(f32 *)0x801F0D80 = 0.5f;
        return result;
    }
}
/* Keep this exit shared until late scheduling; a direct return gives the
 * bounds checks an earlier equivalent return stub instead of the retail one. */
block_61:
    *(f32 *)0x801F0D80 = 0.0f;
    goto block_128;
block_62:
    *var_a3_248 = signed_invalid;
    *var_a2_247 = invalid_timer;
    *var_t0_245 = 0;
block_63:
    var_t0_245 += 1;
    var_a2_247 += 1;
    var_s1_236 += 1;
    var_a3_248 += 1;
    if ((s32) var_s1_236 < 8) {
        goto loop_34;
    }
block_64:
    temp_v1_402 = *(u8 *)0x801F1037;
    s0_value = 0;
    if (temp_v1_402 == 1) {
        goto block_67;
    }
    if (temp_v1_402 != 2) {
        goto block_75;
    }
    s0_value = (0 - (func_00139D74() == 0)) & 3;
block_67:
    if (func_00139C88() != 0) {
        goto block_69;
    }
    s0_value = 2;
    goto block_71;
block_69:
block_71:
    if (func_00139CE8() != 0) {
        goto block_73;
    }
    s0_value = 1;
    goto block_75;
block_73:
block_75:
    if (s0_value == 0) {
        goto block_88;
    }
    if (s0_value != 1) {
        goto block_80;
    }
    os_inval_icache(overlay_b_icache_text_start, overlay_b_icache_data_start - overlay_b_icache_text_start);
    os_inval_dcache(overlay_b_dcache_data_start, overlay_b_dcache_bss_start - overlay_b_dcache_data_start);
    func_0002de50(overlay_b_dma_rom_start, overlay_b_dma_text_start, overlay_b_dma_rom_end - overlay_b_dma_rom_start);
    if (overlay_b_bss_start == overlay_b_bss_end) {
        goto block_79;
    }
    memset_00023780(overlay_b_bss_start, overlay_b_bss_end - overlay_b_bss_start);
block_79:
    *(void **)0x801F0DE0 = 0x1C;
    *(s32 *)0x801F0DE8 = -1;
    D_801F365D = (u8)(D_801F365D | 1);
    goto block_87;
block_80:
    if (s0_value == 2) {
        goto block_82;
    }
    goto block_84;
block_82:
    func_0012EA80(-1, position);
    goto block_86;
block_84:
    if (s0_value != 3) {
        goto block_87;
    }
    func_0012EA80(func_00130C2C(), position);
block_86:
    func_0010D484(position[0], position[1], position[2], D_8018F58C, 0x29);
block_87:
    func_0005c090(0x21, 1);
    D_801F365D = (u8)(D_801F365D & 0xFD);
    return 1;
block_88:
    temp_v1_508 = *(u8 *)0x801F1036;
    if (temp_v1_508 == 1) {
        goto block_91;
    }
    if (temp_v1_508 == 2) {
        goto block_92;
    }
    goto block_96;
block_91:
    s0_value = func_00139C10();
    goto block_96;
block_92:
    if (*(u8 *)0x801F0FDE != 0) {
        goto block_94;
    }
    s0_value = 1;
    goto block_96;
block_94:
block_96:
    if (func_0005c110(0x11) != 0) {
        return 0;
    }
    eligibility = (*(u8 *)0x8018F481 != 0x3F) & (s0_value != 0);
    if (eligibility == 0) {
        goto block_return_zero;
    }
    func_0005c090(0x21, 1);
    temp_v0_541 = func_00130C2C();
    if (temp_v0_541 == -1) {
        goto block_100;
    }
    temp_v1_548 = temp_v0_541 * 0x24;
    (*(s16 *)((s8 *)((temp_v1_548 + 0x80190000)) + (0x51CC))) = (s16) (((*(u16 *)((s8 *)((temp_v1_548 + 0x80190000)) + (0x51CC))) & 0xFFFB) | 2);
block_100:
    os_inval_icache(overlay_c_icache_text_start, overlay_c_icache_data_start - overlay_c_icache_text_start);
    os_inval_dcache(overlay_c_dcache_data_start, overlay_c_dcache_bss_start - overlay_c_dcache_data_start);
    func_0002de50(overlay_c_dma_rom_start, overlay_c_dma_text_start, overlay_c_dma_rom_end - overlay_c_dma_rom_start);
    if (overlay_c_bss_start == overlay_c_bss_end) {
        goto block_102;
    }
    memset_00023780(overlay_c_bss_start, overlay_c_bss_end - overlay_c_bss_start);
block_102:
    temp_v1_588 = *(u16 *)0x801936B8;
    *(s8 *)0x8021C74C = 1;
    if (temp_v1_588 == 0) {
        goto block_104;
    }
    *(u16 *)0x8021C740 = temp_v1_588;
    goto block_105;
block_104:
    *(u16 *)0x8021C740 = *(u16 *)0x801936BA;
block_105:
    {
        s32 amount;
        s32 threshold_value;
        s32 base_value;
        *(u8 *)0x8021C748 = func_00127C50();
        amount = func_00127BFC();
        threshold_value = *(u8 *)0x801936A9;
        base_value = *(u16 *)(0x801E7E1A + (*(u8 *)0x8018F481 * 0xC));
        if (threshold_value < 50) {
            D_8021C744 = (((50 - threshold_value) > 0 ? (50 - threshold_value) : (threshold_value - 50)) * amount / 25) + base_value;
        } else {
            D_8021C744 = (((threshold_value - 50) > 0 ? (threshold_value - 50) : (50 - threshold_value)) * amount / 50) + base_value;
        }
    }
    if (*(u8 *)0x8018F481 == 0x36) {
        goto block_119;
    }
    money = &D_80196A6C;
    temp_v0_695 = *money + *(s32 *)0x8021C744;
    *money = temp_v0_695;
    if (temp_v0_695 > 0x98967FU) {
        *money = 0x98967F;
    }
block_116:
block_118:
    func_00130E60(*(u16 *)0x8021C740, 0);
block_119:
    *(u16 *)0x801936B8 = 0;
    var_a3_248 = (s8 *)0xC8;
    if (*(u8 *)0x8018F481 == 0x36) {
        goto block_124;
    }
    reward_pair = D_80190F80;
    temp_a1_716 = reward_pair[0];
    temp_a0_718 = *(u8 *)0x8021C748;
    /* Keep the byte view in this comparison and the integer view below.
     * Earlier commoning otherwise reuses the comparison subtraction. */
    if (((s32)var_a3_248 - (u8)temp_a1_716) >= (s32) temp_a0_718) {
        goto block_122;
    }
    {
        s32 b1;
        b1 = D_80190F80[1];
        b1 += (s32)var_a3_248 - temp_a1_716;
        reward_pair[0] = 0xC8;
        D_80190F80[1] = b1;
    }
    goto block_124;
block_122:
    reward_pair[0] = temp_a1_716 + temp_a0_718;
    D_80190F80[1] += temp_a0_718;
/* Fresh scalar roles retain the flags-read/word-read order found in retail. */
block_124: {
    u8 flags;
    u8 *final_state;
    s32 final_word;
    flags = D_801F365D;
    final_state = D_800EB0DC;
    final_word = *(s32 *)final_state;
    *(s32 *)D_801F0DE0 = 0x1B;
    D_801F0DE8 = -1;
    D_801F365D = (u8)(flags | 1);
    if (final_word != 0xB) {
        func_800EA6B0(final_state - 0x2C, 0xA, 0xA, (s32)var_a3_248);
    }
    if (D_8018F481 != 0x36) {
        return 1;
    }
    func_800EA6C8(final_state - 0x2C);
}
block_128:
    return 1;
block_return_zero:
    return 0;
}
