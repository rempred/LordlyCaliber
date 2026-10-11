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
M2C_UNK func_0020019C();
M2C_UNK func_0020BD5C(M2C_UNK, u8);
s32 func_0020BFE4();
s32 func_0020C32C(s32);
s32 func_0020C788();
s32 func_0020C810();
M2C_UNK func_002159D0(void);
M2C_UNK func_0021C3B0();
M2C_UNK func_801EFAAC();
M2C_UNK func_00023460(void *, void *, M2C_UNK);
void *func_80070F30(M2C_UNK, void *, s32);
M2C_UNK func_800712C4(void *);

extern u16 D_800E8100;
extern u8 D_801976DC;
extern u8 *D_801CE8BC;
extern u8 *D_801CE8C0;
extern u8 D_801CEAAB;
extern s8 D_801CEAAC;
extern s8 D_801CEAAD;
extern s8 D_801CEAAE;
extern u16 D_801D0758;
extern s16 D_801D075A;
extern s16 D_801D075C;
extern s16 D_801D075E;
extern s16 D_801D0760;

void func_00217BA8(void) {
    s32 temp_s2_120;
    s32 temp_v0_41;
    s32 temp_v0_55;
    s32 temp_v1_71;
    s32 var_a2_84;
    u16 temp_v0_107;
    u16 temp_v0_94;
    u16 *timer_x;
    u8 temp_a1_185;
    u8 temp_s1_138;
    u8 temp_v0_223;
    u8 temp_v0_251;
    void *temp_a0_178;
    void *temp_a0_88;
    u8 *temp_a1_131;
    u8 *temp_s0_128;
    void *temp_v1_211;
    void *temp_v1_248;
    void *temp_v1_50;
    void *temp_v1_64;
    void *temp_v1_8;
    void *var_a0_222;

    temp_v1_8 = D_801CE8BC;
    if ((*(s32 *)((s8 *)(temp_v1_8) + (0x6048))) == 0) {
        D_801D0758 = 0x143;
        D_801D075A = 0x10;
        D_801D075C = -0x83;
        D_801D075E = 0x98;
        D_801D0760 = 0;
        D_801CEAAB = 0;
        D_801CEAAE = 0;
        D_801CEAAD = 0;
        D_801CEAAC = 0;
        (*(u8 *)((s8 *)(temp_v1_8) + (0x6089))) = 0U;
        D_801CE8BC[0x608A] = 0U;
        temp_v0_41 = func_0020C810();
        if ((temp_v0_41 != 0) && (func_0020C32C(temp_v0_41) != 0)) {
            temp_v1_50 = D_801CE8BC;
            (*(u8 *)((s8 *)(temp_v1_50) + (0x6089))) = (u8) ((*(u8 *)((s8 *)(temp_v1_50) + (0x6089))) | 4);
        }
        temp_v0_55 = func_0020C788();
        if ((temp_v0_55 != 0) && (func_0020C32C(temp_v0_55) != 0)) {
            temp_v1_64 = D_801CE8BC;
            (*(u8 *)((s8 *)(temp_v1_64) + (0x608A))) = (u8) ((*(u8 *)((s8 *)(temp_v1_64) + (0x608A))) | 4);
        }
    }
    temp_v1_71 = *(s32 *)(D_801CE8BC + 0x6048);
    if (temp_v1_71 < 0x10) {
        D_801CEAAB = (u8)(temp_v1_71 * 0xC);
        goto block_23;
    }
    if (func_0020BFE4() != 0) {
        var_a2_84 = 0x3C;
    } else {
        var_a2_84 = 0x1E;
    }
    temp_a0_88 = D_801CE8BC;
    timer_x = &D_801D0758;
    temp_v0_94 = *timer_x;
    temp_v0_94 += 0xE;
    temp_v0_94 -= *(s32 *)((s8 *)temp_a0_88 + 0x6048);
    *timer_x = temp_v0_94;
    if ((s16) temp_v0_94 < 0xAA) {
        *timer_x = 0xAAU;
    }
    temp_v0_107 = D_801D075C;
    temp_v0_107 -= 0xE;
    temp_v0_107 += *(s32 *)((s8 *)temp_a0_88 + 0x6048);
    D_801D075C = temp_v0_107;
    if ((s16) temp_v0_107 >= 0x17) {
        D_801D075C = 0x16U;
    }
    temp_s2_120 = var_a2_84 + 0x10;
    (*(s32 *)((s8 *)(temp_a0_88) + (0x604C))) = (s32) ((*(s32 *)((s8 *)(temp_a0_88) + (0x604C))) | 1);
    if ((*(s32 *)((s8 *)(temp_a0_88) + (0x6048))) == temp_s2_120) {
        temp_s0_128 = func_80070F30(0x6094, timer_x, var_a2_84);
        temp_a1_131 = D_801CE8BC;
        /* Transfer experiment from 158E4: identical real operations in both
         * arms; the owner bit has no inferred meaning. Keep the owner read
         * after allocation and preserve the allocator's existing arguments.
         */
        if ((u32)temp_a1_131 & 1) {
            *(s32 *)(D_801CE8C0 + 0x814) = 0;
            func_00023460(temp_a1_131, temp_s0_128, 0x6094);
            temp_s1_138 = D_801CE8C0[0x82E];
        } else {
            *(s32 *)(D_801CE8C0 + 0x814) = 0;
            func_00023460(temp_a1_131, temp_s0_128, 0x6094);
            temp_s1_138 = D_801CE8C0[0x82E];
        }
        func_801EFAAC();
        D_801CE8C0[0x82E] = temp_s1_138;
        func_00023460(temp_s0_128 + 0x1C4, D_801CE8BC + 0x1C4, 0x1360);
        func_00023460(temp_s0_128 + 0x1524, D_801CE8BC + 0x1524, 0x3C00);
        func_00023460(temp_s0_128 + 0x5124, D_801CE8BC + 0x5124, 0x19C);
        func_00023460(temp_s0_128 + 0x52C0, D_801CE8BC + 0x52C0, 0x400);
        *(s32 *)(D_801CE8BC + 0x56C0) = *(s32 *)((s8 *)temp_s0_128 + 0x56C0);
        func_800712C4(temp_s0_128);
        func_0021C3B0();
        func_0020019C();
    }
    temp_a0_178 = D_801CE8BC;
    if (temp_s2_120 < (*(s32 *)((s8 *)(temp_a0_178) + (0x6048)))) {
        temp_a1_185 = D_801CEAAB;
        (*(s32 *)((s8 *)(temp_a0_178) + (0x604C))) = (s32) ((*(s32 *)((s8 *)(temp_a0_178) + (0x604C))) & ~1);
        D_801D0760 = 1;
        if (temp_a1_185 < 5U) {
            D_801CEAAB = 0U;
        } else {
            D_801CEAAB = (u8)(temp_a1_185 - 5);
        }
        if (D_801CEAAB == 0) {
            func_0020BD5C(1, temp_a1_185);
            return;
        }
        goto block_23;
    }
block_23:
    temp_v1_211 = D_801CE8BC;
    if ((*(s32 *)((s8 *)(temp_v1_211) + (0x6048))) >= 0x10) {
        if (D_800E8100 & 0x8010) {
            var_a0_222 = temp_v1_211 + (*(u8 *)((s8 *)(temp_v1_211) + (0x6072)));
            temp_v0_223 = (*(u8 *)((s8 *)(var_a0_222) + (0x606E)));
            if (((temp_v0_223 != 2) & (temp_v0_223 != 9)) && (D_801976DC < 0x1EU)) {
                (*(u8 *)((s8 *)(var_a0_222) + (0x606E))) = 1U;
                D_801CE8BC[0x608B] = 0;
                goto block_31;
            }
        }
        if (D_800E8100 & 0x2020) {
            temp_v1_248 = D_801CE8BC;
            var_a0_222 = temp_v1_248 + (*(u8 *)((s8 *)(temp_v1_248) + (0x6072)));
            temp_v0_251 = (*(u8 *)((s8 *)(var_a0_222) + (0x606E)));
            if ((temp_v0_251 != 2) & (temp_v0_251 != 9)) {
                (*(u8 *)((s8 *)(var_a0_222) + (0x606E))) = 1U;
                D_801CE8BC[0x608B] = 1;
block_31:
                func_002159D0();
            }
        }
    }
    *(s32 *)(D_801CE8BC + 0x6048) = *(s32 *)(D_801CE8BC + 0x6048) + 1;
}
