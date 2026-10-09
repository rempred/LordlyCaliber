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
M2C_UNK func_001F0E64(void *, M2C_UNK);                         
f32 func_001FF7EC(s16, s16);                                    
f32 func_002016F4(s32, s32);                                    
M2C_UNK func_0020BD5C(M2C_UNK);                                 
s32 func_0020BFF8(void **);                                     
s32 func_0020C014(void **);                                     
s32 func_0020C0CC(void **);                                     
s32 func_0020C2C0(void **);                                     
s32 func_0020C32C(void **);                                     
void **func_0020C478(s32);                                      
M2C_UNK func_0021824C(s32 *, s32 *, s32 *, s32 *);              
extern void *D_801CE8BC;
extern u8 D_801CEAAB;

void func_00218B58(void)
{
    s32 sp10;
    s32 sp14;
    s32 sp18;
    s32 sp1C;
    M2C_UNK var_a1_89;
    s32 temp_a0_22;
    s32 first_one;
    s32 later_one;
    s32 later_two;
    s32 var_s1_131;
    s32 var_s3_221;
    s32 var_s2_24;
    s32 var_v1_72;
    void **temp_v0_138;
    void **temp_v0_27;
    s8 *var_s0_270;
    s8 *var_s0_40;
    void *temp_a0_352;
    void *temp_a0_91;
    void *temp_v0_335;
    void *temp_v1_288;
    void *temp_v1_296;
    void *temp_v1_305;
    void *temp_v1_313;
    void *temp_v1_371;

    func_0021824C(&sp10, &sp14, &sp18, &sp1C);
    temp_a0_22 = (*(s32 *)((s8 *)D_801CE8BC + (0x6048)));
    var_s2_24 = 0;
    if (temp_a0_22 != 0) {
        goto block_28;
    }
    first_one = 1;
loop_2:
    temp_v0_27 = func_0020C478(var_s2_24);
    if (func_0020C2C0(temp_v0_27) != 0) {
        goto block_26;
    }
    if (func_0020C32C(temp_v0_27) != 0) {
        goto block_26;
    }
    if ((*(u16 *)((s8 *)(temp_v0_27) + (0x20))) != 0) {
        var_s0_40 = (s8 *) temp_v0_27;
        goto block_10;
    }
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6087))) != first_one) {
        goto block_7;
    }
    if (func_0020C014(temp_v0_27) != 0) {
        var_s0_40 = (s8 *) temp_v0_27;
        goto block_10;
    }
block_7:
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6087))) != 2) {
        goto block_26;
    }
    if (func_0020BFF8(temp_v0_27) == 0) {
        goto block_26;
    }
    var_s0_40 = (s8 *) temp_v0_27;
block_10:
    var_s3_221 = (s32) (var_s0_40 + 0xC);
loop_11:
    if (*(void **) var_s0_40 == 0) {
        goto block_25;
    }
    if (func_0020C014(temp_v0_27) == 0) {
        goto block_16;
    }
    var_v1_72 = sp10;
    if ((u32) (var_v1_72 - 1) < 2U) {
        goto block_22;
    }
block_16:
    if (func_0020BFF8(temp_v0_27) != 0) {
        goto block_18;
    }
    goto block_25;
block_18:
    var_v1_72 = sp14;
    if ((u32) (var_v1_72 - 1) >= 2U) {
        goto block_25;
    }
block_22:
    var_a1_89 = 5;
    temp_a0_91 = (s8 *) (*(void **) var_s0_40) + 0x44;
    if (var_v1_72 != first_one) {
        goto block_24;
    }
    var_a1_89 = 0xB;
block_24:
    func_001F0E64(temp_a0_91, var_a1_89);
block_25:
    var_s0_40 += 4;
    if ((u32) var_s0_40 < (u32) var_s3_221) {
        goto loop_11;
    }
block_26:
    var_s2_24 += 1;
    if (var_s2_24 >= 0x14) {
        goto block_82;
    }
    goto loop_2;
block_28:
    if (sp18 == 0) {
        goto block_35;
    }
    if (D_801CEAAB == 0xFF) {
        goto block_52;
    }
    var_s3_221 = temp_a0_22 - 0x10;
    temp_a0_22 = var_s3_221 * 4;
    temp_a0_22 &= (s32) ~temp_a0_22 >> 0x1F;
    if (temp_a0_22 < 0x100) {
        goto block_32;
    }
    temp_a0_22 = 0xFF;
    goto block_34;
block_32:
block_34:
    D_801CEAAB = (u8) temp_a0_22;
    goto block_53;
block_35:
    var_s1_131 = 1;
    if (sp10 != 0) {
        goto block_37;
    }
    if (sp14 == 0) {
        goto block_52;
    }
block_37:
    var_s3_221 = 0;
loop_38:
    temp_v0_138 = func_0020C478(var_s3_221);
    if (func_0020C2C0(temp_v0_138) != 0) {
        goto block_50;
    }
    if (func_0020C32C(temp_v0_138) != 0) {
        goto block_50;
    }
    if ((*(u16 *)((s8 *)(temp_v0_138) + (0x20))) != 0) {
        goto block_45;
    }
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6087))) != 1) {
        goto block_43;
    }
    if (func_0020C014(temp_v0_138) != 0) {
        goto block_45;
    }
block_43:
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6087))) != 2) {
        goto block_50;
    }
    if (func_0020BFF8(temp_v0_138) == 0) {
        goto block_50;
    }
block_45:
    if (func_0020C014(temp_v0_138) == 0) {
        goto block_47;
    }
    if (((sp10 == 0) | (sp10 == 3)) != 0) {
        goto block_50;
    }
block_47:
    if (func_0020BFF8(temp_v0_138) == 0) {
        goto block_49;
    }
    if (((sp14 == 0) | (sp14 == 3)) != 0) {
        goto block_50;
    }
block_49:
    var_s1_131 &= 0 - ((u32) (((*(u16 *)((s8 *)((*(void **)((s8 *)(temp_v0_138) + (0)))) + (0x1C))) + 0x18F) & 0xFFFF) >= 0x2BBU);
block_50:
    var_s3_221 += 1;
    if (var_s3_221 < 0x14) {
        goto loop_38;
    }
    if (var_s1_131 == 0) {
        goto block_53;
    }
block_52:
    func_0020BD5C(0xA);
    return;
block_53:
    var_s3_221 = 0;
    if (sp10 != 0) {
        goto block_55;
    }
    if (sp14 == 0) {
        goto block_82;
    }
block_55:
    later_one = 1;
    later_two = 2;
loop_56:
    temp_v0_27 = func_0020C478(var_s3_221);
    if (func_0020C2C0(temp_v0_27) != 0) {
        goto block_81;
    }
    if (func_0020C32C(temp_v0_27) != 0) {
        goto block_81;
    }
    if (func_0020C0CC(temp_v0_27) != 0) {
        goto block_81;
    }
    if ((*(u16 *)((s8 *)(temp_v0_27) + (0x20))) != 0) {
        goto block_64;
    }
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6087))) != later_one) {
        goto block_62;
    }
    if (func_0020C014(temp_v0_27) != 0) {
        goto block_64;
    }
block_62:
    if ((*(u8 *)((s8 *)D_801CE8BC + (0x6087))) != later_two) {
        goto block_81;
    }
    if (func_0020BFF8(temp_v0_27) == 0) {
        goto block_81;
    }
block_64:
    var_s2_24 = (s32) (func_002016F4((*(s32 *)((s8 *)(temp_v0_27) + (0x48))), (*(s32 *)((s8 *)(temp_v0_27) + (0x4C)))) / 2.0f);
    var_s0_270 = (s8 *) temp_v0_27;
loop_65:
    if ((*(void **)((s8 *)(var_s0_270) + (0))) == 0) {
        goto block_80;
    }
    if (func_0020C014(temp_v0_27) == 0) {
        goto block_71;
    }
    if (sp10 != later_one) {
        goto block_69;
    }
    temp_v1_288 = (*(void **)((s8 *)(var_s0_270) + (0)));
    (*(u16 *)((s8 *)(temp_v1_288) + (0x1C))) = (u16) ((*(u16 *)((s8 *)(temp_v1_288) + (0x1C))) + var_s2_24);
block_69:
    if (sp10 != later_two) {
        goto block_79;
    }
    temp_v1_296 = (*(void **)((s8 *)(var_s0_270) + (0)));
    (*(u16 *)((s8 *)(temp_v1_296) + (0x1C))) = (u16) ((*(u16 *)((s8 *)(temp_v1_296) + (0x1C))) - var_s2_24);
    goto block_79;
block_71:
    if (sp14 != later_one) {
        goto block_73;
    }
    temp_v1_305 = (*(void **)((s8 *)(var_s0_270) + (0)));
    (*(u16 *)((s8 *)(temp_v1_305) + (0x1C))) = (u16) ((*(u16 *)((s8 *)(temp_v1_305) + (0x1C))) - var_s2_24);
block_73:
    if (sp14 != later_two) {
        goto block_75;
    }
    temp_v1_313 = (*(void **)((s8 *)(var_s0_270) + (0)));
    (*(u16 *)((s8 *)(temp_v1_313) + (0x1C))) = (u16) ((*(u16 *)((s8 *)(temp_v1_313) + (0x1C))) + var_s2_24);
block_75:
    if (sp18 == 0) {
        goto block_79;
    }
    if (sp1C != 0) {
        goto block_79;
    }
    if (sp14 == 0) {
        goto block_79;
    }
    var_v1_72 = *(u8 *)((s8 *)(temp_v0_27) + (0xAB));
    var_v1_72 -= 4;
    if (var_v1_72 < 0) {
        var_v1_72 = 0;
    }
    (*(u8 *)((s8 *)(temp_v0_27) + (0xAC))) = (u8) var_v1_72;
    (*(u8 *)((s8 *)(temp_v0_27) + (0xAB))) = (u8) var_v1_72;
block_79:
    temp_v0_335 = (*(void **)((s8 *)(var_s0_270) + (0)));
    (*(u16 *)((s8 *)((*(void **)((s8 *)(var_s0_270) + (0)))) + (0x1E))) = (u16) (s32) func_001FF7EC((s16) (*(u16 *)((s8 *)(temp_v0_335) + (0x1C))), (*(s16 *)((s8 *)(temp_v0_335) + (0x20))));
    (*(u16 *)((s8 *)((*(void **)((s8 *)(var_s0_270) + (0xC)))) + (0x1C))) = (u16) (*(u16 *)((s8 *)((*(void **)((s8 *)(var_s0_270) + (0)))) + (0x1C)));
    (*(u16 *)((s8 *)((*(void **)((s8 *)(var_s0_270) + (0xC)))) + (0x1E))) = (u16) (*(u16 *)((s8 *)((*(void **)((s8 *)(var_s0_270) + (0)))) + (0x1E)));
    temp_a0_352 = (*(void **)((s8 *)(var_s0_270) + (0)));
    (*(s32 *)((s8 *)(temp_a0_352) + (4))) = (s32) ((s16) (*(u16 *)((s8 *)(temp_a0_352) + (0x1C))) * 0x1E);
block_80:
    var_s0_270 += 4;
    if ((u32) var_s0_270 < (u32) ((s8 *) temp_v0_27 + 0xC)) {
        goto loop_65;
    }
block_81:
    var_s3_221 += 1;
    if (var_s3_221 < 0x14) {
        goto loop_56;
    }
block_82:
    temp_v1_371 = D_801CE8BC;
    (*(s32 *)((s8 *)(temp_v1_371) + (0x6048))) = (s32) ((*(s32 *)((s8 *)(temp_v1_371) + (0x6048))) + 1);
    return;
}
