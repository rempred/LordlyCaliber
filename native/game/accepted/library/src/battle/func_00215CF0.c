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
s32 func_000453e0(s32);
s32 func_00045df4(s16);
M2C_UNK func_00054e24();
M2C_UNK func_00055114(M2C_UNK, M2C_UNK, s8, M2C_UNK, s32, s32, s32, s32, s32, s32, s32, s32);
void *func_00055234();
s32 func_001F21A4(u8, void *, void *);
M2C_UNK func_00208DC8();
M2C_UNK func_0020BD5C(M2C_UNK);
s32 func_0020BF6C();
M2C_UNK func_002159D0(void);
M2C_UNK func_0021AE6C(f32, M2C_UNK, void *);
M2C_UNK func_0021C3B0();
M2C_UNK func_801EFAAC();
M2C_UNK func_00023460(void *, void *, M2C_UNK);
void *func_80070F30(M2C_UNK);
M2C_UNK func_800712C4(void *);
M2C_UNK func_000238B0(void *, const void *, s32);
M2C_UNK func_0002C950(void *, const void *);

extern const f64 D_801E66A8;
extern const f64 D_801E66B0;
extern const f64 D_801E66B8;
extern u16 D_800E8100;
extern u16 D_800E8700;
extern u8 D_800E9C12;
extern u8 D_800EB1F0[];
extern u8 *D_8018FDC0;
extern u8 D_801936E0;
extern u8 D_801976DC;
extern u8 *D_801CE8BC;
extern u8 *D_801CE8C0;
extern s32 D_801CE8C4;
extern u8 D_801CE902[];
extern s16 D_801CE904[];
extern u8 D_80197207[];
extern u8 D_801E6694[];

void func_00215CF0(void) {
    u8 sp30[0x100];
    M2C_UNK var_a1_247;
    M2C_UNK var_a1_272;
    s32 temp_v0_236;
    s32 temp_v1_493;
    s32 var_a0_resource;
    s32 var_a0_271;
    s32 var_a2_287;
    s32 var_a3_274;
    s32 var_a3_state3;
    s32 var_s0_379;
    s32 var_s0_752;
    s32 var_s1_755;
    s8 temp_v1_539;
    u16 temp_a0_122;
    u16 temp_v1_141;
    u16 temp_v1_596;
    u16 temp_v1_758;
    u32 temp_a0_477;
    u32 temp_v1_902;
    u8 temp_s0_172;
    u8 temp_s1_335;
    u8 temp_s1_860;
    u8 temp_v0_145;
    u8 temp_v0_417;
    u8 temp_v1_153;
    s32 temp_v1_15;
    u8 temp_v1_435;
    u8 temp_v1_77;
    u8 var_v0_253;
    void *temp_a1_14;
    void *temp_a1_853;
    void *temp_a2_410;
    void *temp_a2_76;
    void *temp_a2_8;
    void *temp_s0_325;
    void *temp_s0_653;
    void *temp_s0_850;
    void *temp_v0_217;
    void *temp_v0_47;
    void *temp_v0_517;
    void *temp_v0_533;
    void *temp_v0_615;
    void *temp_v0_664;
    void *temp_v0_836;
    void *temp_v1_189;
    void *temp_v1_302;
    void *temp_v1_503;
    void *temp_v1_606;
    void *temp_v1_618;
    void *temp_v1_684;
    void *temp_v1_81;
    void *temp_v1_827;
    void *temp_v1_dec4;
    void *temp_v1_dec5;
    void *temp_v1_dec6;
    void *var_a0_247;
    void *var_v1_251;

    temp_a2_8 = D_801CE8BC;
    temp_a1_14 = temp_a2_8 + (*(u8 *)((s8 *)(temp_a2_8) + (0x6072)));
    temp_v1_15 = (*(u8 *)((s8 *)(temp_a1_14) + (0x606E)));
    if (temp_v1_15 >= 2U) {
        goto state_2;
    }
    if (temp_v1_15 != 0) {
        goto block_10;
    }
        if ((D_800E8100 & 0x8010) && (D_801976DC < 0x1EU)) {
            (*(u8 *)((s8 *)(temp_a1_14) + (0x606E))) = 1U;
            (*(u8 *)((s8 *)(D_801CE8BC) + (0x608B))) = 0U;
            goto block_10;
        }
        if (!(D_800E8100 & 0x2020)) {
            D_801CE8C4 = 1;
            return;
        }
        temp_v0_47 = D_801CE8BC;
        (*(s8 *)((s8 *)((temp_v0_47 + (*(u8 *)((s8 *)(temp_v0_47) + (0x6072))))) + (0x606E))) = 1;
        (*(u8 *)((s8 *)(D_801CE8BC) + (0x608B))) = 1U;
block_10:
        func_002159D0();
        return;
state_2:
        if (temp_v1_15 != 2) {
            goto state_3;
        }
        if (D_801CE8C4 == 0) {
            func_0021AE6C(1.0f, 0, temp_a2_8);
            (*(s8 *)((s8 *)(D_801CE8BC) + (0x608C))) = 0;
            temp_a2_76 = D_801CE8BC;
            temp_v1_77 = (*(u8 *)((s8 *)(temp_a2_76) + (0x608B)));
            if (temp_v1_77 == 0) {
                temp_v1_81 = temp_a2_76 + (*(u8 *)((s8 *)(temp_a2_76) + (0x6072)));
                (*(u8 *)((s8 *)(temp_v1_81) + (0x606E))) = (u8) ((*(u8 *)((s8 *)(temp_v1_81) + (0x606E))) + 1);
            } else if (temp_v1_77 == 1) {
                (*(s8 *)((s8 *)((temp_a2_76 + (*(u8 *)((s8 *)(temp_a2_76) + (0x6072))))) + (0x606E))) = 0xB;
                func_00055114(0x20003, 0xA, D_800E9C12 == 0, -2, 0x78, 0x75, 0xC8, 0x87, 0x50, 0x6B, 0xF0, 0x91);
            }
            goto block_123;
        }
        return;
state_3:
        if (temp_v1_15 != 3) {
            goto state_4;
        }
        temp_a0_122 = D_800E8100;
        if (temp_a0_122 & 0x4000) {
            (*(u8 *)((s8 *)(temp_a1_14) + (0x606E))) = 0U;
            (*(u8 *)((s8 *)(D_801CE8BC) + (0x6072))) = 0U;
            D_801CE8C4 = 1;
            func_00208DC8(D_800EB1F0, 1, temp_a2_8);
            var_a0_resource = 0x6094;
            goto block_122;
        }
        temp_v1_141 = D_800E8700;
        if (temp_v1_141 & 0x200) {
            temp_v0_145 = (*(u8 *)((s8 *)(temp_a2_8) + (0x6073)));
            if (temp_v0_145 == 0) {
                temp_v0_145 = 2;
            } else {
                temp_v0_145 -= 1;
            }
            goto block_25;
        }
        if (temp_v1_141 & 0x100) {
            temp_v1_153 = (*(u8 *)((s8 *)(temp_a2_8) + (0x6073)));
            if (temp_v1_153 < 2U) {
                temp_v0_145 = temp_v1_153 + 1;
block_25:
                (*(u8 *)((s8 *)(temp_a2_8) + (0x6073))) = temp_v0_145;
            } else {
                (*(u8 *)((s8 *)(temp_a2_8) + (0x6073))) = 0U;
            }
            func_00208DC8(D_800EB1F0, 2, temp_a2_8);
            return;
        }
        if (!(temp_a0_122 & 0x8000)) {
            goto block_54;
        }
        {
            temp_s0_172 = (*(u8 *)((s8 *)(temp_a2_8) + (0x6073)));
            if (temp_s0_172 == 0) {
                if (func_0020BF6C(temp_a0_122, temp_a1_14, temp_a2_8) != 0) {
                    (*(s8 *)((s8 *)(D_801CE8BC) + (0x606E))) = 0;
                    (*(u8 *)((s8 *)(D_801CE8BC) + (0x6072))) = 0U;
                    func_00208DC8(D_800EB1F0, 0);
                    temp_v1_189 = D_801CE8BC;
                    (*(u32 *)((s8 *)(temp_v1_189) + (0x1B0)))--;
                    if ((f64) (*(f32 *)((s8 *)(temp_v1_189) + (0x1AC))) == D_801E66A8) {
                        (*(f32 *)((s8 *)(temp_v1_189) + (0x1AC))) = 0.0f;
                    }
                    func_0020BD5C(6);
                    return;
                }
                if ((*(u32 *)((s8 *)(D_801CE8BC) + (0x1B0))) != 0) {
                    if (func_00055234(0xA) == 0) {
                        temp_v0_217 = D_801CE8BC;
                        temp_v0_236 = (*(u8 *)((s8 *)(temp_v0_217) + (0x6073))) * 0x12;
                        var_a3_state3 = 0x6D;
                        func_00055114(0x20000, 0xA, (*(s8 *)((s8 *)(temp_v0_217) + (0x1C3))), -2, temp_v0_236 + 0x85, var_a3_state3, temp_v0_236 + 0x97, 0x83, 0x72, 0x55, 0xCE, 0x9B);
                        var_v1_251 = D_801CE8BC;
                        var_v0_253 = (*(u8 *)((s8 *)(var_v1_251) + (0x6072)));
                        var_a0_247 = D_800EB1F0;
                        var_a1_247 = 0;
                        var_v0_253 += 1;
                        (*(u8 *)((s8 *)(var_v1_251) + (0x6072))) = var_v0_253;
                        var_v1_251 += var_v0_253 & 0xFF;
                        var_v0_253 = 4;
                        goto block_95;
                    }
                } else {
                    goto block_48;
                }
            } else if (temp_s0_172 == 1) {
                if ((u32) (*(u32 *)((s8 *)(temp_a2_8) + (0x1B0))) >= 2U) {
                    if (D_801936E0 != 0) {
                        if (func_00055234(0x10B) == 0) {
                            var_a0_271 = 0x31033;
                            var_a1_272 = 0xB;
                            var_a3_274 = 0x6D;
                            var_a2_287 = ((*(u8 *)((s8 *)(D_801CE8BC) + (0x6073))) * 0x12) + 0x85;
                            func_00054e24(var_a0_271, var_a1_272, var_a2_287, var_a3_274, var_a2_287 + 0x12, 0x83, 0x1E, 0x1E, 0x122, 0x46);
                            goto block_after_93;
                        }
                    } else {
                        (*(s8 *)((s8 *)(D_801CE8BC) + (0x606E))) = 0;
                        (*(u8 *)((s8 *)(D_801CE8BC) + (0x6072))) = 0U;
                        func_00208DC8(D_800EB1F0, 0, temp_a2_8);
                        temp_v1_302 = D_801CE8BC;
                        (*(u32 *)((s8 *)(temp_v1_302) + (0x1B0))) -= 2;
                        if ((f64) (*(f32 *)((s8 *)(temp_v1_302) + (0x1AC))) == D_801E66B0) {
                            (*(f32 *)((s8 *)(temp_v1_302) + (0x1AC))) = 0.0f;
                        }
                        (*(s8 *)((s8 *)(D_801CE8BC) + (0x6087))) = 1;
                        D_801CE8C4 = (s32) temp_s0_172;
                        temp_s0_325 = func_80070F30(0x6094);
                        /* KMC workaround: this word-array clear and direct owner
                         * argument restore retail instruction order at this site.
                         * The allocator still precedes the owner read.
                         */
                        ((s32 *)D_801CE8C0)[0x205] = 0;
                        func_00023460(D_801CE8BC, temp_s0_325, 0x6094);
                        temp_s1_335 = (*(u8 *)((s8 *)(D_801CE8C0) + (0x82E)));
                        func_801EFAAC();
                        (*(u8 *)((s8 *)(D_801CE8C0) + (0x82E))) = temp_s1_335;
                        func_00023460(temp_s0_325 + 0x1C4, D_801CE8BC + 0x1C4, 0x1360);
                        func_00023460(temp_s0_325 + 0x1524, D_801CE8BC + 0x1524, 0x3C00);
                        func_00023460(temp_s0_325 + 0x5124, D_801CE8BC + 0x5124, 0x19C);
                        func_00023460(temp_s0_325 + 0x52C0, D_801CE8BC + 0x52C0, 0x400);
                        (*(s32 *)((s8 *)(D_801CE8BC) + (0x56C0))) = (s32) (*(s32 *)((s8 *)(temp_s0_325) + (0x56C0)));
                        func_800712C4(temp_s0_325);
                        func_0021C3B0();
                        func_0020BD5C(1);
                        return;
                    }
                } else {
                    goto block_48;
                }
            } else {
                var_s0_379 = 0;
                if ((u32) (*(u32 *)((s8 *)(temp_a2_8) + (0x1B0))) >= 3U) {
                    goto block_50;
                }
block_48:
                func_00208DC8(D_800EB1F0, 7);
                return;
block_49:
                (*(s8 *)((s8 *)(D_801CE8BC) + (0x57DC))) = var_s0_379;
                goto block_53;
block_50:
                var_a3_state3 = 0;
                (*(s8 *)((s8 *)(D_801CE8BC) + (0x57DC))) = 0;
loop_51:
                if (func_00045df4(*(s16 *)((u8 *)D_801CE904 + var_a3_state3)) != 0) {
                    goto block_49;
                }
                var_s0_379 += 1;
                var_a3_state3 += 6;
                if (var_s0_379 < 6) {
                    goto loop_51;
                }
block_53:
                temp_a2_410 = D_801CE8BC;
                (*(s32 *)((s8 *)(temp_a2_410) + (0x57D8))) = 0xFF;
                temp_v0_417 = (*(u8 *)((s8 *)(temp_a2_410) + (0x6072))) + 1;
                (*(u8 *)((s8 *)(temp_a2_410) + (0x6072))) = temp_v0_417;
                temp_a2_410 += temp_v0_417 & 0xFF;
                (*(s8 *)((s8 *)(temp_a2_410) + (0x606E))) = 7;
                func_00208DC8(D_800EB1F0, 0, temp_a2_410);
                return;
            }
        }
        return;
block_54:
        if (!(temp_a0_122 & 0x1000)) {
            return;
        }
        if (func_00055234(0x10B, temp_a1_14, temp_a2_8) != 0) {
            return;
        }
        var_a3_state3 = 0x6D;
        temp_v1_435 = (*(u8 *)((s8 *)(D_801CE8BC) + (0x6073)));
        var_a2_287 = (temp_v1_435 * 0x12) + 0x85;
        if (temp_v1_435 != 0) {
            goto block_58;
        }
        if (func_0020BF6C() != 0) {
            goto block_59;
        }
block_58:
        var_a0_271 = ((*(u8 *)((s8 *)(D_801CE8BC) + (0x6073))) + 0x32) | 0x31000;
        goto block_60;
block_59:
        var_a0_271 = 0x31035;
block_60:
        var_a1_272 = 0xB;
        func_00054e24(var_a0_271, var_a1_272, var_a2_287, var_a3_state3, var_a2_287 + 0x12, var_a3_state3 + 0x16, 0x1E, 0x1E, 0x122, 0x46);
        goto block_after_93;
        return;
state_4:
        if (temp_v1_15 != 4) {
            goto state_5;
        }
        temp_a0_477 = (*(u32 *)((s8 *)(D_8018FDC0) + (0xD4)));
        if (temp_a0_477 == -2U) {
            (*(u8 *)((s8 *)(temp_a1_14) + (0x606E))) = 6U;
            return;
        }
        if (temp_a0_477 < 4U) {
            (*(u32 *)((s8 *)(temp_a2_8) + (0x1C0))) = temp_a0_477;
            temp_v1_493 = D_801976DC * 0x19;
            if (D_80197207[temp_v1_493] != temp_a0_477) {
                D_80197207[temp_v1_493] = (s8) temp_a0_477;
                temp_v1_503 = D_801CE8BC;
                (*(u32 *)((s8 *)(temp_v1_503) + (0x1B0)))--;
                if ((f64) (*(f32 *)((s8 *)(temp_v1_503) + (0x1AC))) == D_801E66B8) {
                    (*(f32 *)((s8 *)(temp_v1_503) + (0x1AC))) = 0.0f;
                }
            }
            temp_v0_517 = D_801CE8BC;
            (*(s8 *)((s8 *)((temp_v0_517 + (*(u8 *)((s8 *)(temp_v0_517) + (0x6072))))) + (0x606E))) = 0xA;
            return;
        }
        if ((D_800E8100 & 0x1000) && (func_00055234(0x10C, temp_a1_14, temp_a2_8) == 0)) {
            temp_v0_533 = func_00055234(0x100A);
            if (temp_v0_533 != 0) {
                var_a1_272 = 0xC;
                temp_v1_539 = (*(s8 *)((s8 *)(temp_v0_533) + (0x22)));
                var_a2_287 = 0x78;
                var_a0_271 = (temp_v1_539 + 0xF) | 0x32000;
                var_a3_274 = (temp_v1_539 * 0x14) + 0x5F;
                func_00054e24(var_a0_271, var_a1_272, var_a2_287, var_a3_274, 0xC8, var_a3_274, 0x1E, 0x1E, 0x122, 0x46);
                goto block_after_93;
            }
            temp_v1_dec4 = D_801CE8BC;
            (*(s8 *)((s8 *)(temp_v1_dec4) + (0x6072))) = (*(u8 *)((s8 *)(temp_v1_dec4) + (0x6072))) - 1;
            return;
        }
        return;
state_5:
        if (temp_v1_15 != 5) {
            goto state_6;
        }
        if (func_00055234(0x10C, temp_a1_14, temp_a2_8) != 0) {
            return;
        }
        if (func_00055234(0x10B) != 0) {
            return;
        }
        temp_v1_dec5 = D_801CE8BC;
        (*(s8 *)((s8 *)(temp_v1_dec5) + (0x6072))) = (*(u8 *)((s8 *)(temp_v1_dec5) + (0x6072))) - 1;
        return;
state_6:
        if (temp_v1_15 != 6) {
            goto state_7;
        }
        if (func_00055234(0x100A, temp_a1_14, temp_a2_8) != 0) {
            return;
        }
        temp_v1_dec6 = D_801CE8BC;
        (*(s8 *)((s8 *)(temp_v1_dec6) + (0x6072))) = (*(u8 *)((s8 *)(temp_v1_dec6) + (0x6072))) - 1;
        return;
state_7:
        if (temp_v1_15 != 7) {
            goto state_8_or_later;
        }
        temp_v1_596 = D_800E8100;
        if (temp_v1_596 & 0x8000) {
            if (func_001F21A4((u8) (*(s8 *)((s8 *)(temp_a2_8) + (0x57DC))), temp_a1_14, temp_a2_8) != 0) {
                temp_v1_606 = D_801CE8BC;
                (*(s32 *)((s8 *)(temp_v1_606) + (0x57D8))) = 0;
                (*(s16 *)((s8 *)(temp_v1_606) + (0x6076))) = (s16) ((u8) (*(s8 *)((s8 *)(temp_v1_606) + (0x57DC))) | 0x8000);
                func_00208DC8(D_800EB1F0, 0);
                temp_v0_615 = D_801CE8BC;
                (*(u8 *)((s8 *)(temp_v0_615) + (0x6072))) = 0U;
                temp_v1_618 = D_801CE8BC;
                (*(u32 *)((s8 *)(temp_v0_615) + (0x1B0))) = 0U;
                (*(f32 *)((s8 *)(temp_v0_615) + (0x1AC))) = 0.0f;
                (*(s8 *)((s8 *)((temp_v1_618 + (*(u8 *)((s8 *)(temp_v1_618) + (0x6072))))) + (0x606E))) = 0;
                var_a0_resource = 0x6094;
                D_801CE8C4 = 1;
                goto block_122;
            }
            if (func_00055234(0x10B) == 0) {
                func_00054e24(0x46077, 0xB, 0x96, 0x28, 0xAA, 0x3C, 0x1E, 0x1E, 0x122, 0x46);
                temp_s0_653 = func_00055234(0x10B);
                func_000238B0(sp30, D_801E6694, func_000453e0((u8) (*(s8 *)((s8 *)(D_801CE8BC) + (0x57DC))) + 0x27));
                func_0002C950((*(void **)((s8 *)(temp_s0_653) + (0xD0))), sp30);
                temp_v0_664 = (*(void **)((s8 *)(temp_s0_653) + (0xD0)));
                (*(void **)((s8 *)(temp_v0_664) + (0x530))) = temp_v0_664;
                (*(s8 *)((s8 *)((*(void **)((s8 *)(temp_s0_653) + (0xD0)))) + (0x5F5))) = 1;
                var_v1_251 = D_801CE8BC;
                var_v0_253 = (*(u8 *)((s8 *)(var_v1_251) + (0x6072)));
                var_a0_247 = D_800EB1F0;
                var_a1_247 = 9;
                var_v0_253 += 1;
                goto block_94_after_increment;
            }
        } else {
            if (!(temp_v1_596 & 0x4000)) {
                goto block_90;
            }
            (*(s32 *)((s8 *)(temp_a2_8) + (0x57D8))) = 0;
            func_00208DC8(D_800EB1F0, 1, temp_a2_8);
            temp_v1_684 = D_801CE8BC;
            (*(s16 *)((s8 *)(temp_v1_684) + (0x6076))) = 0;
            (*(s8 *)((s8 *)(temp_v1_684) + (0x6072))) = (*(u8 *)((s8 *)(temp_v1_684) + (0x6072))) - 1;
            return;
block_90:
            if (!(temp_v1_596 & 0x1000)) {
                goto block_96;
            }
            if (func_00055234(0x10C) != 0) {
                return;
            }
            var_a1_272 = 0xC;
            var_a2_287 = 0x96;
            var_a3_274 = 0x28;
            var_a0_271 = D_801CE902[(u8) (*(s8 *)(D_801CE8BC + 0x57DC)) * 6] | 0x3F000;
            func_00054e24(var_a0_271, var_a1_272, var_a2_287, var_a3_274, 0xAA, 0x3C, 0x1E, 0x1E, 0x122, 0x46);
block_after_93:
            var_v1_251 = D_801CE8BC;
            var_a0_247 = D_800EB1F0;
            var_a1_247 = 0;
            var_v0_253 = (*(u8 *)((s8 *)(var_v1_251) + (0x6072)));
            var_v0_253 += 1;
block_94_after_increment:
            (*(s8 *)((s8 *)(var_v1_251) + (0x6072))) = var_v0_253;
            var_v1_251 += var_v0_253 & 0xFF;
            var_v0_253 = 5;
block_95:
            (*(s8 *)((s8 *)(var_v1_251) + (0x606E))) = var_v0_253;
            func_00208DC8(var_a0_247, var_a1_247);
            return;
block_96:
            if (!(D_800E8700 & 0xC00)) {
                return;
            }
            var_s0_752 = (u8) (*(s8 *)((s8 *)(D_801CE8BC) + (0x57DC)));
            var_s1_755 = var_s0_752 * 6;
loop_98:
            temp_v1_758 = D_800E8700;
            if (temp_v1_758 & 0x800) {
                goto block_100;
            }
            goto block_104;
block_100:
            var_s1_755 -= 6;
            if (var_s0_752 == 0) {
                goto block_103;
            }
            var_s0_752 -= 1;
            goto block_108;
block_103:
            var_s1_755 = 0x1E;
            var_s0_752 = 5;
            goto block_108;
block_104:
            if (!(temp_v1_758 & 0x400)) {
                goto block_108;
            }
            var_s1_755 += 6;
            if (var_s0_752 >= 5) {
                goto block_107;
            }
            var_s0_752 += 1;
            goto block_108;
block_107:
            var_s1_755 = 0;
            var_s0_752 = 0;
block_108:
            if (func_00045df4(*(s16 *)((u8 *)D_801CE904 + var_s1_755)) != 0) {
                goto block_110;
            }
            if ((u8) (*(s8 *)(D_801CE8BC + 0x57DC)) != var_s0_752) {
                goto loop_98;
            }
block_110:
            if ((u8) (*(s8 *)((s8 *)(D_801CE8BC) + (0x57DC))) == var_s0_752) {
                return;
            }
            func_00208DC8(D_800EB1F0, 2);
            (*(s8 *)((s8 *)(D_801CE8BC) + (0x57DC))) = (s8) var_s0_752;
            return;
        }
        return;
state_8_or_later:
        if (temp_v1_15 == 8) {
            return;
        }
        if (temp_v1_15 == 9) {
            return;
        }
        if (temp_v1_15 != 10) {
            goto state_11;
        }
        if (func_00055234(0x100A, temp_a1_14, temp_a2_8) == 0) {
            goto block_120;
        }
        return;
state_11:
        if (temp_v1_15 != 11) {
            return;
        }
        if (func_00055234(0x100A, temp_a1_14, temp_a2_8) != 0) {
            goto state_11_tail;
        }
        temp_v1_827 = D_801CE8BC;
        (*(u8 *)((s8 *)(temp_v1_827) + (0x6072))) = (u8) ((*(u8 *)((s8 *)(temp_v1_827) + (0x6072))) - 1);
block_120:
        (*(u8 *)((s8 *)(D_801CE8BC) + (0x6072))) = 0U;
        temp_v0_836 = D_801CE8BC;
        (*(s8 *)((s8 *)((temp_v0_836 + (*(u8 *)((s8 *)(temp_v0_836) + (0x6072))))) + (0x606E))) = 0;
        var_a0_resource = 0x6094;
        D_801CE8C4 = 1;
block_122:
        temp_s0_850 = func_80070F30(var_a0_resource);
        temp_a1_853 = D_801CE8BC;
        /* KMC scheduling workaround: identical operations in both arms.
         * The test disappears; no meaning is assigned to this owner bit.
         */
        if ((u32)temp_a1_853 & 1) {
            (*(s32 *)((s8 *)(D_801CE8C0) + (0x814))) = 0;
            func_00023460(temp_a1_853, temp_s0_850, 0x6094);
            temp_s1_860 = (*(u8 *)((s8 *)(D_801CE8C0) + (0x82E)));
        } else {
            (*(s32 *)((s8 *)(D_801CE8C0) + (0x814))) = 0;
            func_00023460(temp_a1_853, temp_s0_850, 0x6094);
            temp_s1_860 = (*(u8 *)((s8 *)(D_801CE8C0) + (0x82E)));
        }
        func_801EFAAC();
        (*(u8 *)((s8 *)(D_801CE8C0) + (0x82E))) = temp_s1_860;
        func_00023460(temp_s0_850 + 0x1C4, D_801CE8BC + 0x1C4, 0x1360);
        func_00023460(temp_s0_850 + 0x1524, D_801CE8BC + 0x1524, 0x3C00);
        func_00023460(temp_s0_850 + 0x5124, D_801CE8BC + 0x5124, 0x19C);
        func_00023460(temp_s0_850 + 0x52C0, D_801CE8BC + 0x52C0, 0x400);
        (*(s32 *)((s8 *)(D_801CE8BC) + (0x56C0))) = (s32) (*(s32 *)((s8 *)(temp_s0_850) + (0x56C0)));
        func_800712C4(temp_s0_850);
block_123:
        func_0021C3B0();
        return;
state_11_tail:
        temp_v1_902 = (*(u32 *)((s8 *)(D_8018FDC0) + (0xD4)));
        if (temp_v1_902 < 2U) {
            D_800E9C12 = (u8) (temp_v1_902 == 0);
        }
        return;
}
