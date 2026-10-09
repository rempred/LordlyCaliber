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
M2C_UNK func_00041674();
M2C_UNK func_0005a0f4();
M2C_UNK func_001F0B40();
M2C_UNK func_001F1E18();
M2C_UNK func_001FFE80();
M2C_UNK func_002071F4();
M2C_UNK func_00208D80();
M2C_UNK func_00208DC8(M2C_UNK, M2C_UNK);
M2C_UNK func_00208EB0();
M2C_UNK func_0020BD5C(M2C_UNK);
s32 func_0020BF6C();
s32 func_0020BF7C();
s32 func_0020BFF8(void *);
s32 func_0020C2C0(void *);
s32 func_0020C32C(void *);
void *func_0020C478(s32);
void *func_0020C788();
void *func_0020C810(s32);
M2C_UNK func_00215CF0();
M2C_UNK func_00217BA8();
M2C_UNK func_0021840C();
M2C_UNK func_00218B58();
M2C_UNK func_00219470();
M2C_UNK func_00219A14();
M2C_UNK func_00219F0C();
M2C_UNK func_0021A438();
M2C_UNK func_0021A5C8();
M2C_UNK func_0021A9B4();
M2C_UNK func_0021B0A0();
M2C_UNK func_0021B2E0();
M2C_UNK func_0021B438();
M2C_UNK func_0021B770();
M2C_UNK func_0021C3B0();
M2C_UNK func_002215D0();
M2C_UNK func_002224F4();
M2C_UNK func_00226AFC();
M2C_UNK func_0025FCB4();
M2C_UNK memset_00023780(void *, M2C_UNK);
extern u8 *volatile D_801CE8BC;
extern u16 D_800E8100;
extern u8 D_800EB1F0[];
extern u8 D_80193670;
extern u8 D_80193671;
extern u8 D_80193672;
extern u8 D_80193673;
extern u8 D_801936E0;
extern u8 D_80195560[];
extern u8 D_801976D8;
extern u8 D_801976E8;
extern u8 g_func_001957D0_source_records[];

u16 func_0021B894(s32 arg0) {
    s32 temp_a0_535;
    s32 var_a0_0;
    s32 var_a1_294;
    s32 var_s1_459;
    s32 var_v1_295;
    u16 temp_v1_276;
    u16 var_s3_33;
    s32 temp_s0_30;
    u8 temp_v0_156;
    u8 temp_v0_528;
    u8 var_s2_513;
    u8 var_v0_378;
    u8 var_v0_417;
    void *temp_a0_234;
    void *temp_a0_80;
    void *temp_s1_525;
    void *temp_v0_341;
    void *temp_v0_399;
    void *temp_v0_461;
    void *temp_v1_144;
    void *temp_v1_22;
    void *var_v0_150;
    u8 *fill_base;
    volatile u8 *flag_ptr;

    temp_v1_22 = D_801CE8BC;
    temp_s0_30 = (*(u8 *)((s8 *)(temp_v1_22) + (0x6044)));
    var_s3_33 = 0;
    if ((s32) temp_s0_30 >= 9) {
        goto block_5;
    }
    if ((s32) temp_s0_30 >= 2) {
        goto block_23;
    }
    if (temp_s0_30 == 0) {
        goto block_11;
    }
    if (temp_s0_30 == 1) {
        goto block_18;
    }
    goto block_92;
block_5:
    if ((s32) temp_s0_30 >= 0xD) {
        goto block_9;
    }
    if ((s32) temp_s0_30 >= 0xB) {
        goto block_23;
    }
    if (temp_s0_30 == 0xA) {
        goto block_44;
    }
    goto block_92;
block_9:
    if (temp_s0_30 == 0x64) {
        goto block_91;
    }
    goto block_92;
block_11:
    func_00208D80();
    func_002071F4();
    func_001F0B40();
    if (!(D_800E8100 & 0x1000)) {
        goto block_17;
    }
    if (func_0020BF7C() != 0) {
        goto block_17;
    }
    if (D_801936E0 != 0) {
        goto block_17;
    }
    temp_a0_80 = D_801CE8BC;
    if ((*(u8 *)((s8 *)(temp_a0_80) + (0x6044))) != 1) {
        goto block_17;
    }
    if ((*(u8 *)((s8 *)((temp_a0_80 + (*(u8 *)((s8 *)(temp_a0_80) + (0x6072))))) + (0x606E))) != 0) {
        goto block_17;
    }
    D_801976D8 = (u8) (D_801976D8 ^ 0x20);
    func_00208DC8(D_800EB1F0, 0x2D6);
block_17:
    func_0021B0A0();
    func_002215D0();
    func_00226AFC();
    func_0025FCB4();
    func_0021B438();
    func_0021B2E0();
    func_001FFE80();
    func_001F1E18();
    func_0005a0f4();
    func_00208EB0();
    goto block_92;
block_18:
    func_00208D80();
    func_00215CF0();
    func_002071F4();
    func_001F0B40();
    if (!(D_800E8100 & 0x1000)) {
        goto block_42;
    }
    if (func_0020BF7C() != 0) {
        goto block_42;
    }
    if (D_801936E0 != 0) {
        goto block_42;
    }
    temp_v1_144 = D_801CE8BC;
    if ((*(u8 *)((s8 *)(temp_v1_144) + (0x6044))) != temp_s0_30) {
        goto block_42;
    }
    var_v0_150 = temp_v1_144 + (*(u8 *)((s8 *)(temp_v1_144) + (0x6072)));
    goto block_40;
block_23:
    func_00208D80();
    temp_v0_156 = (*(u8 *)((s8 *)D_801CE8BC + (0x6044)));
    if ((u32) (temp_v0_156 - 2) >= 0xBU) {
        goto block_34;
    }
    switch (temp_v0_156) {
case 2:
    func_00217BA8();
    goto block_34;
case 3:
    func_0021840C();
    goto block_34;
case 4:
    func_00218B58();
    goto block_34;
case 5:
    func_00219470();
    goto block_34;
case 6:
    func_00219A14();
    goto block_34;
case 7:
    func_00219F0C();
    goto block_34;
case 8:
    func_0021A438();
    goto block_34;
case 11:
    func_0021A5C8();
    goto block_34;
case 12:
    func_0021A9B4();
    }
block_34:
    func_002071F4();
    func_001F0B40();
    if ((*(s32 *)((s8 *)D_801CE8BC + (0x604C))) & 1) {
        goto block_43;
    }
    if (!(D_800E8100 & 0x1000)) {
        goto block_42;
    }
    if (func_0020BF7C() != 0) {
        goto block_42;
    }
    if (D_801936E0 != 0) {
        goto block_42;
    }
    temp_a0_234 = D_801CE8BC;
    if ((*(u8 *)((s8 *)(temp_a0_234) + (0x6044))) != 1) {
        goto block_42;
    }
    var_v0_150 = temp_a0_234 + (*(u8 *)((s8 *)(temp_a0_234) + (0x6072)));
block_40:
    if ((*(u8 *)((s8 *)(var_v0_150) + (0x606E))) != 0) {
        goto block_42;
    }
    D_801976D8 = (u8) (D_801976D8 ^ 0x20);
    func_00208DC8(D_800EB1F0, 0x2D6);
block_42:
    func_0021B0A0();
    func_002215D0();
    func_00226AFC();
    func_0025FCB4();
    func_0021B438();
    func_0021B2E0();
    func_001FFE80();
block_43:
    func_001F1E18();
    func_00208EB0();
    goto block_92;
block_44:
    temp_v1_276 = (*(u16 *)((s8 *)(temp_v1_22) + (0x606A)));
    *(s8 *)0x801CEAAB = 0;
    *(s8 *)0x801CEAAA = 0;
    *(s8 *)0x801CEAA9 = 0;
    *(s8 *)0x801CEAA8 = 0;
    var_s3_33 = 2;
    if (temp_v1_276 & 0x800) {
        goto block_52;
    }
    var_s3_33 = 1;
    if (temp_v1_276 != 0) {
        goto block_52;
    }
    if (func_0020BF6C() != 0) {
        var_a0_0 = 1;
        var_a1_294 = 0;
        var_v1_295 = 0x38;
loop_48:
        var_a0_0 += 1;
        var_a1_294 += (*(u8 *)((s8 *)((var_v1_295 + 0x80190000)) + (0x3BD1))) != 0;
        var_v1_295 += 0x38;
        if (var_a0_0 < 0x64) {
            goto loop_48;
        }
        var_s3_33 = 5;
        if (var_a1_294 >= 0x5F) {
            var_s3_33 = 6;
        }
    } else {
        var_a0_0 = 1;
        var_s3_33 = 5;
    }
block_52:
    if ((*(u16 *)((s8 *)D_801CE8BC + (0x6068))) != 0) {
        goto block_54;
    }
    flag_ptr = &D_80193672;
    *flag_ptr = (u8) (*flag_ptr | 1);
block_54:
    if ((*(u16 *)((s8 *)D_801CE8BC + (0x6068))) != 1) {
        goto block_56;
    }
    flag_ptr = &D_80193673;
    *flag_ptr = (u8) (*flag_ptr | 1);
block_56:
    flag_ptr = &D_80193670;
    *flag_ptr = (u8) (*flag_ptr | 1);
    temp_v0_341 = func_0020C810(var_a0_0);
    if (temp_v0_341 == 0) {
        goto block_69;
    }
    if ((*(u16 *)((s8 *)(temp_v0_341) + (0x20))) != 0) {
        goto block_60;
    }
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6089))) & 4) {
        goto block_60;
    }
    D_80193672 = (u8) (D_80193672 | 2);
    goto block_69;
block_60:
    if (temp_v0_341 == 0) {
        goto block_69;
    }
    if (func_0020C32C(temp_v0_341) == 0) {
        goto block_64;
    }
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6089))) & 4) {
        goto block_64;
    }
    flag_ptr = &D_80193672;
    var_v0_378 = *flag_ptr | 4;
    goto block_68;
block_64:
    if (temp_v0_341 == 0) {
        goto block_69;
    }
    if (func_0020C32C(temp_v0_341) != 0) {
        goto block_69;
    }
    if (!((*(u8 *)((s8 *)D_801CE8BC + (0x6089))) & 4)) {
        goto block_69;
    }
    flag_ptr = &D_80193672;
    var_v0_378 = *flag_ptr | 8;
block_68:
    *flag_ptr = var_v0_378;
block_69:
    temp_v0_399 = func_0020C788();
    if (temp_v0_399 == 0) {
        goto block_82;
    }
    if ((*(u16 *)((s8 *)(temp_v0_399) + (0x20))) != 0) {
        goto block_73;
    }
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x608A))) & 4) {
        goto block_73;
    }
    flag_ptr = &D_80193673;
    var_v0_417 = *flag_ptr | 2;
    goto block_81;
block_73:
    if (temp_v0_399 == 0) {
        goto block_82;
    }
    if (func_0020C32C(temp_v0_399) == 0) {
        goto block_77;
    }
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x608A))) & 4) {
        goto block_77;
    }
    flag_ptr = &D_80193673;
    var_v0_417 = *flag_ptr | 4;
    goto block_81;
block_77:
    if (temp_v0_399 == 0) {
        goto block_82;
    }
    if (func_0020C32C(temp_v0_399) != 0) {
        goto block_82;
    }
    if (!((*(u8 *)((s8 *)D_801CE8BC + (0x608A))) & 4)) {
        goto block_82;
    }
    flag_ptr = &D_80193673;
    var_v0_417 = *flag_ptr | 8;
block_81:
    *flag_ptr = var_v0_417;
block_82:
    if (func_0020BF6C() == 0) {
        goto block_90;
    }
    for (var_s1_459 = 0; var_s1_459 < 0x14; var_s1_459++) {
        temp_v0_461 = func_0020C478(var_s1_459);
        if (temp_v0_461 == 0) {
            continue;
        }
        if (func_0020BFF8(temp_v0_461) == 0) {
            continue;
        }
        if (func_0020C2C0(temp_v0_461) != 0) {
            continue;
        }
        if (func_0020C32C(temp_v0_461) == 0) {
            break;
        }
    }
    if (var_s1_459 < 0x14) {
        goto block_90;
    }
    flag_ptr = &D_80193670;
    *flag_ptr = (u8) (*flag_ptr | 0x10);
block_90:
    func_00041674();
    func_0021B770();
    func_0020BD5C(0x64);
    goto block_92;
block_91:
    return (*(u16 *)((s8 *)(temp_v1_22) + (0x606C)));
block_92:
    if (var_s3_33 == 0) {
        goto block_100;
    }
    func_0021C3B0();
    func_002224F4();
    if (func_0020BF6C() == 0) {
        goto block_99;
    }
    var_s2_513 = 0;
    temp_s0_30 = 0;
    fill_base = D_80195560;
    temp_s1_525 = (D_801976E8 * 0x19) + g_func_001957D0_source_records;
    for (; (u32) temp_s0_30 < 5U; temp_s0_30++) {
        temp_v0_528 = (*(u8 *)((s8 *)(temp_s1_525 + temp_s0_30) + (2)));
        if (temp_v0_528 == 0) {
            continue;
        }
        temp_a0_535 = temp_v0_528 * 0x34;
        var_s2_513 = D_80195560[temp_a0_535 + 0x12];
        temp_a0_535 += (s32) fill_base;
        memset_00023780((void *) temp_a0_535, 0x34);
    }
    memset_00023780(temp_s1_525, 0x19);
    D_80193671 = 0;
    D_801976E8 = var_s2_513;
block_99:
    (*(u16 *)((s8 *)D_801CE8BC + (0x606C))) = var_s3_33;
block_100:
    return var_s3_33;
}
