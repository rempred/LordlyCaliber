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

M2C_UNK func_00208DC8(void *, M2C_UNK);
s32 func_0020BF7C();
s32 func_0020BF8C();
s32 func_0020F80C();
M2C_UNK func_0021C3B0();
s32 func_0021C970();
M2C_UNK func_801EFAAC();
M2C_UNK func_00023460(void *, void *, M2C_UNK);
void *func_80070F30(M2C_UNK);
M2C_UNK func_800712C4(void *);

extern u8 *D_801CE8BC;
extern u8 *D_801CE8C0;
extern u8 D_800EB1F0[];
extern u8 D_801E5C70[];

void func_002159D0(void)
{
    s32 var_a0_75;
    s32 var_s2_17;
    s32 type_38;
    s32 type_41;
    s32 type_16;
    s32 temp_a1_87;
    u8 loop_type;
    u8 temp_s1_32;
    void *temp_a2_84;
    void *allocation;
    void *temp_s0_22;
    void *temp_v0_157;
    void *temp_v0_83;
    void *temp_v1_114;
    void *temp_v1_162;
    void *temp_v1_185;
    u8 *first_marker_ptr;
    u8 *second_marker_ptr;

    var_s2_17 = -1;
    if (*(s32 *)(D_801CE8C0 + 0x814) != 0) {
        goto block_2;
    }
    allocation = func_80070F30(0x6094);
    /* Keep the clear as a word-array element for KMC memory classification.
     * Loading the copy owner directly here avoids an intervening store between
     * its temporary load and argument move. The allocator still runs first.
     */
    ((s32 *)D_801CE8C0)[0x205] = 0;
    func_00023460(D_801CE8BC, temp_s0_22 = allocation, 0x6094);
    temp_s1_32 = D_801CE8C0[0x82E];
    func_801EFAAC();
    D_801CE8C0[0x82E] = temp_s1_32;
    func_00023460(temp_s0_22 + 0x1C4, D_801CE8BC + 0x1C4, 0x1360);
    func_00023460(temp_s0_22 + 0x1524, D_801CE8BC + 0x1524, 0x3C00);
    func_00023460(temp_s0_22 + 0x5124, D_801CE8BC + 0x5124, 0x19C);
    func_00023460(temp_s0_22 + 0x52C0, D_801CE8BC + 0x52C0, 0x400);
    *(s32 *)(D_801CE8BC + 0x56C0) = *(s32 *)((s8 *)temp_s0_22 + 0x56C0);
    func_800712C4(temp_s0_22);
    func_0021C3B0();
block_2:
    if (func_0020F80C() != 0) {
        goto block_24;
    }
    var_a0_75 = *(s32 *)(D_801CE8C0 + 0x810);
    if (var_a0_75 < 0) {
        goto block_24;
    }
    type_38 = 0x38;
    type_16 = 0x16;
    type_41 = 0x41;
loop_5:
    temp_v0_83 = D_801CE8C0;
    temp_a2_84 = temp_v0_83 + var_a0_75;
    temp_a1_87 = *(u8 *)((s8 *)temp_a2_84 + 0x10);
    if (*(u8 *)((s8 *)temp_v0_83 + 0x819) <
        *(u8 *)((s8 *)temp_a2_84 + 0x11)) {
        goto block_7;
    }
    var_s2_17 = -1;
    goto block_23;
block_7:
    if (var_s2_17 >= 0) {
        goto block_9;
    }
    var_s2_17 = var_a0_75;
    goto block_11;
block_9:
block_11:
    if (temp_a1_87 == type_38) {
        goto block_23;
    }
    if (temp_a1_87 < 0x39U) {
        if (temp_a1_87 == type_16) {
            goto block_store_type_38;
        }
    } else {
        loop_type = (u8)temp_a1_87;
        for (;;) {
            if (loop_type != type_41) {
                goto block_20;
            }
            goto block_23;
        }
    }
    goto block_20;
block_store_type_38:
    *(u8 *)((s8 *)temp_a2_84 + 0x10) = type_38;
    goto block_19;
block_19:
    temp_v1_114 = D_801CE8C0 + var_a0_75;
    *(u8 *)((s8 *)temp_v1_114 + 0x13) =
        (u8)(*(u8 *)((s8 *)temp_v1_114 + 0x13) + 4);
    goto block_23;
block_20:
    if (D_801E5C70[temp_a1_87] != 3) {
        goto block_22;
    }
    *(s8 *)(D_801CE8C0 + var_a0_75 + 0x10) = 0x1E;
    goto block_23;
block_22:
    {
        void *field_ptr;

        *(s8 *)(D_801CE8C0 + var_a0_75 + 0x10) = type_38;
        field_ptr = D_801CE8C0;
        field_ptr += var_a0_75;
        *(u8 *)((s8 *)field_ptr + 0x13) = D_801E5C70[temp_a1_87];
    }
block_23:
    var_a0_75 = func_0021C970();
    if (var_a0_75 >= 0) {
        goto loop_5;
    }
block_24:
    if (var_s2_17 < 0) {
        goto block_28;
    }
    *(s8 *)(D_801CE8C0 + var_s2_17 + 0x10) = 0x41;
    temp_v0_157 = D_801CE8C0;
    *(u8 *)((s8 *)(temp_v0_157 + var_s2_17) + 0x11) =
        *(u8 *)((s8 *)temp_v0_157 + 0x819);
    temp_v1_162 = D_801CE8C0;
    {
        s32 selected;

        selected = *(s32 *)((s8 *)temp_v1_162 + 0x810);
        if (selected == var_s2_17) {
            temp_v0_157 = temp_v1_162 + var_s2_17;
            goto block_27;
        }
        temp_v0_157 = temp_v1_162 + var_s2_17;
        *(s8 *)((s8 *)temp_v0_157 + 0x12) = 0;
    }
block_27:
    first_marker_ptr = D_801CE8BC;
    first_marker_ptr += *(u8 *)((s8 *)first_marker_ptr + 0x6072);
    *(s8 *)((s8 *)first_marker_ptr + 0x606E) = 2;
    goto block_after_marker;
block_28:
    if (func_0020BF7C() != 0) {
        goto block_30;
    }
    if (func_0020BF8C() == 0) {
        goto block_33;
    }
block_30:
    temp_v1_185 = D_801CE8BC;
    if (*(u8 *)((s8 *)temp_v1_185 + 0x6086) != 0) {
        goto block_33;
    }
    second_marker_ptr = temp_v1_185 + *(u8 *)((s8 *)temp_v1_185 + 0x6072);
    *(s8 *)((s8 *)second_marker_ptr + 0x606E) = 9;
block_after_marker:
    D_801CE8BC[0x6073] = 0;
    func_00208DC8(D_800EB1F0, 0);
    D_801CE8BC[0x608C] = 1;
    return;
block_33:
    D_801CE8BC[0x606E] = 0;
    D_801CE8BC[0x6072] = 0;
    D_801CE8BC[0x608C] = 0;
    return;
}
