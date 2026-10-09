extern unsigned char D_80214F80_dma[], D_802172B0_data[];
extern unsigned char D_00195410[], D_001977E0[];
extern unsigned char D_80217350_start[], D_80217350_end[];
extern unsigned char D_800E7AB9[];
extern unsigned char D_800EB240[];
extern unsigned char D_8018F481[];
extern unsigned char D_80190000[];
extern unsigned char D_801969B8[];
extern unsigned char D_801969BA[];
extern unsigned char D_801969BF[];
extern unsigned char D_801E85A0[];
extern unsigned char D_801F0000[];
extern unsigned char D_801F0CB0[];
extern unsigned char D_801F0D30[];
extern unsigned char D_801F0D98[];
extern unsigned char D_801F0D9C[];
extern unsigned char D_801F0DA0[];
extern unsigned char D_801F0DA4[];
extern unsigned char D_801F0E18[];
extern unsigned char D_801F0FDE[];
extern unsigned char D_801F367C[];
extern unsigned char D_80214F80[];
extern unsigned char D_802172B0[];
extern unsigned char D_80217350[];
extern unsigned char D_801F0EBF[][18];
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
M2C_UNK func_0002de50(M2C_UNK, M2C_UNK, s32);
M2C_UNK func_0005c090(M2C_UNK, M2C_UNK);
s32 func_0005c110(M2C_UNK);
f32 func_000F315C(u8, f32, f32);
f32 func_000F3428(u8, f32, f32);
M2C_UNK func_00106CE0(void *, void *, f32);
M2C_UNK func_001072B8(void *);
M2C_UNK func_0010746C(void *);
M2C_UNK func_0010766C(void *);
M2C_UNK func_00118D0C(void *);
M2C_UNK func_00120CF0(void *, f32, f32, f32);
M2C_UNK func_00121724(void *, f32, f32, f32);
M2C_UNK func_00121DA8(void *);
s32 func_001237F0(void *);
M2C_UNK func_00124B98(void *);
M2C_UNK func_00125F84(void *, s32, s32 *);
M2C_UNK func_00126D24(void *);
s32 func_00127D30(void *);
M2C_UNK func_00127EAC(void *);
f32 func_00128050(void *);
M2C_UNK func_00128980(void *);
s32 func_00128AD0(void *);
M2C_UNK func_00128BF4(void *, s32, M2C_UNK);
M2C_UNK func_00128D50(void *);
M2C_UNK func_00128DDC(void *);
M2C_UNK func_00128E80();
M2C_UNK func_0012967C(s32);
M2C_UNK func_0012A828(s32);
M2C_UNK func_0012A958(void *);
M2C_UNK func_0012BC64(void *, s32, s32 *, M2C_UNK);
M2C_UNK func_0012D170(void *, u8, s32 *, s32);
s32 func_0012DA10(void *);
s32 func_0012DB2C(void *);
s32 func_0012E8EC(void *);
s32 func_0012E950(u8);
s32 func_0012E968(void *);
M2C_UNK func_0012EA80(M2C_UNK, f32 *);
s32 func_00131480(void *, M2C_UNK);
M2C_UNK func_00147BDC(void *);
M2C_UNK func_001957D0(s32, s32);
M2C_UNK func_00195D9C(s32, M2C_UNK, M2C_UNK, M2C_UNK);
M2C_UNK func_800EA604(M2C_UNK, M2C_UNK);
M2C_UNK func_801CC480(void *, f32, f32, f32, s32);
M2C_UNK memset_00023780(M2C_UNK, s32);
M2C_UNK os_inval_dcache(M2C_UNK, s32);
M2C_UNK os_inval_icache(M2C_UNK, s32);

void func_00105CC0(s32 arg0) {
    void **mainUnits;
    void **lateUnits;
    s32 lateMask, lateTwo, lateOne;
    s32 recordStride;
    u8 *memberBase;
    s32 memberClearMask, memberTestMask, memberSetMask;
    s32 memberOffset;
    /* Stack-layout reconstruction; reserved bytes have no established semantic role. */
    struct {
        f32 position[3];
        u8 reserved24[4];
        s32 sp28;
        u8 reserved2c[11];
        u8 sp37;
        u8 reserved38[16];
    } locals;
    u8 tailOne;
    f32 rangeX, rangeZ;
    f32 mapZ, mapX, mapDenominator;
    f32 currentAngle;
    f32 temp_f0_434;
    f32 temp_f2_776;
    f32 temp_f4_1006;
    f32 temp_f4_978;
    f32 temp_f4_995;
    f32 temp_f6_786;
    f32 temp_f8_993;
    f32 var_f0_1068;
    s32 *temp_a0_762;
    s32 *temp_v1_758;
    s32 temp_a0_260;
    s32 temp_a0_297;
    s32 temp_a0_39;
    s32 temp_a0_592;
    s32 temp_a0_754;
    s32 temp_s0_940;
    s32 temp_v0_1044;
    s32 temp_v0_1057;
    s32 temp_v0_189;
    s32 temp_v0_218;
    s32 temp_v0_332;
    s32 temp_v0_381;
    s32 temp_v0_439;
    s32 temp_v0_955;
    s32 temp_v1_763;
    s32 temp_v1_847;
    s32 absentIndex;
    f32 copiedX, copiedY, copiedZ;
    s32 copiedField64;
    u8 *countPointer;
    s32 expectedEntryType;
    s32 initialCount;
    s32 innerCount;
    s32 var_a1_116;
    s32 var_s0_579;
    s32 var_s1_832;
    s32 var_s2_898;
    s32 var_s3_13;
    s32 var_v0_178;
    s32 var_v0_29;
    s32 var_v0_669;
    s32 var_v0_886;
    s32 var_v0_935;
    s32 var_v1_517;
    s8 *var_s0_831;
    u8 temp_a0_514;
    u8 temp_a0_750;
    u8 temp_s0_708;
    u8 temp_v0_329;
    u8 temp_v0_410;
    u8 temp_v0_431;
    u8 temp_v0_483;
    s32 temp_v0_66;
    u8 temp_v1_1054;
    u8 temp_v1_172;
    u8 temp_v1_90;
    void **var_v1_880;
    void *temp_s1_34;
    void *temp_s1_882;

    mainUnits = (void **)D_801F0CB0;
    var_s3_13 = 0;
    locals.sp37 = 0;
    locals.sp28 = 0;
    tailOne = 1;
    if (*(s32 *)((u32)D_801F367C) > 0) {
        do {
            s32 shiftOne = 1;
            f32 negativeOne = -1.0f;
            var_v0_29 = var_s3_13 * 4;
            temp_s1_34 = mainUnits[var_s3_13];
            if ((u8) (*(u8 *)((s8 *)(temp_s1_34) + (4))) >= 0x1EU) {
                temp_a0_39 = (*(s32 *)((s8 *)(temp_s1_34) + (0)));
                if ((temp_a0_39 & 0x1010) == 0x10) {
                    if (temp_a0_39 & 0x20000000) {
                        func_00121DA8(temp_s1_34);
                        (*(s32 *)((s8 *)(temp_s1_34) + (0))) = (s32) ((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0xDFFFFFFF);
                    } else if (((temp_a0_39 & 0xC0) != 0x40) && (func_0012DA10(temp_s1_34) != 0) && !((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0x08000000) && (temp_v0_66 = func_001237F0(temp_s1_34), (temp_v0_66 != 0))) {
                        if (temp_v0_66 == 0xFF) {
                            func_0010746C(temp_s1_34);
                            (*(s32 *)((s8 *)(temp_s1_34) + (0))) = (s32) ((*(s32 *)((s8 *)(temp_s1_34) + (0))) & ~0x10);
                            func_800EA604(((u32)D_800EB240), 0x289);
                            func_00118D0C(temp_s1_34);
                            func_0012A958(temp_s1_34);
                        } else {
                            temp_v1_90 = (*(u8 *)((s8 *)(temp_s1_34) + (0xBA)));
                            if ((temp_v1_90 >= 0x14U) || (D_801F0EBF[temp_v1_90 - 4][0] == 2)) {
                                initialCount = *(u8 *)D_801F0E18;
                                temp_a0_260 = 0;
                                (*(u8 *)((s8 *)((void *)((u32)D_801F0FDE)) + (0))) = (u8) ((*(u8 *)((s8 *)((void *)((u32)D_801F0FDE)) + (0))) + 1);
                                if (initialCount > 0) {
                                    expectedEntryType = 9;
                                    countPointer = D_801F0FDE - 0x1C6;
                                    innerCount = *(countPointer += 0xA2, countPointer - 0xA2);
                                    var_a1_116 = 0;
                                    do {
                                        if ((*(u8 *)((s8 *)((var_a1_116 + ((u32)D_801F0000))) + (0xE1A))) == expectedEntryType) {
                                            (*(u16 *)countPointer) = (u16) ((*(u16 *)countPointer) & ~(shiftOne << temp_a0_260));
                                        }
                                        temp_a0_260 += 1;
                                        var_a1_116 += 0xA;
                                    } while (temp_a0_260 < innerCount);
                                }
                            }
                            (*(u8 *)((s8 *)(temp_s1_34) + (0xBA))) = temp_v0_66;
                            func_00121DA8(temp_s1_34);
                        }
                    } else {
                        func_00124B98(temp_s1_34);
                        goto block_21;
                    }
                } else {
block_21:
                    if ((u8) (*(u8 *)((s8 *)(temp_s1_34) + (4))) >= 0x1EU) {
                        if (!((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0x11)) {
                            func_00147BDC(temp_s1_34);
                            func_0012A828(var_s3_13);
                        } else {
                            goto block_26;
                        }
                    } else {
                        goto block_24;
                    }
                }
            } else {
block_24:
                if (!((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0x11)) {
                    func_00147BDC(temp_s1_34);
                } else {
block_26:
                    temp_a0_260 = (*(s32 *)((s8 *)(temp_s1_34) + (0)));
                    if (temp_a0_260 & 0x10) {
                        temp_v1_172 = (*(u8 *)((s8 *)(temp_s1_34) + (4)));
                        if ((temp_v1_172 >= 0x1EU) && ((temp_a0_260 & 0x01040000) || ((temp_a0_260 & 8) && (temp_a0_260 & 0x40002)))) {
                            temp_v0_189 = temp_v1_172 * 25;
                            (*(s8 *)((s8 *)((temp_v0_189 + ((u32)D_80190000))) + (0x71F1))) = (s8) ((*(u8 *)((s8 *)((temp_v0_189 + ((u32)D_80190000))) + (0x71F1))) | 8);
                        }
                        if ((((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0x201000) != 0x200000) && ((((*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) & 0xF0) != 0x10) || ((*(u8 *)((s8 *)(temp_s1_34) + (0xB9))) == 0))) {
                            if (func_0012DA10(temp_s1_34) == 0) {
                                temp_v0_218 = (*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) & 0xF0;
                                if ((temp_v0_218 != 0x10) & (temp_v0_218 != 0x30)) {
                                    (*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) = 0x40U;
                                }
                            }
                            if (((*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) & 0xF0) == 0x40) {
                                if (func_0012DA10(temp_s1_34) != 0) {
                                    (*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) = 0U;
                                }
                            }
                            if (!((*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) & 0xF0) && ((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0x08000000)) {
                                (*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) = 0x20U;
                            }
                            if (((*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) & 0xF0) == 0x20) {
                                if (!((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0x08000000)) {
                                    (*(u8 *)((s8 *)(temp_s1_34) + (0xB8))) = 0U;
                                }
                            }
                            temp_a0_260 = (*(s32 *)((s8 *)(temp_s1_34) + (0)));
                            if (!(temp_a0_260 & 0x40)) {
                                if (temp_a0_260 & 0x1000) {
                                    func_00126D24(temp_s1_34);
                                    goto block_154;
                                }
                                if ((temp_a0_260 & 0x02000000) && ((*(s32 *)((s8 *)(temp_s1_34) + (0x70))) != 7)) {
                                    func_0012EA80(-1, &locals.position[0]);
                                    (*(s32 *)((s8 *)(temp_s1_34) + (0))) = (s32) ((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0xFF7FFFFF);
                                    func_00121724(temp_s1_34, locals.position[0], locals.position[1], locals.position[2]);
                                    (*(s32 *)((s8 *)(temp_s1_34) + (0))) = (s32) ((*(s32 *)((s8 *)(temp_s1_34) + (0))) & 0xFDFFFFFF);
                                    goto block_154;
                                }
                                temp_a0_297 = (*(s32 *)((s8 *)(temp_s1_34) + (0)));
                                if (!(temp_a0_297 & 0x20000)) {
                                    if ((u8) (*(u8 *)((s8 *)(temp_s1_34) + (4))) >= 0x1EU) {
                                        if ((*(s32 *)((s8 *)(temp_s1_34) + (0x70))) != 7) {
                                            goto block_64;
                                        }
                                        if (temp_a0_297 & 0x800000) {
                                            func_0010766C(temp_s1_34);
                                            (*(u8 *)((s8 *)(temp_s1_34) + (0x20))) = 0U;
                                            do { func_00125F84(temp_s1_34, arg0, &locals.sp28); } while (0);
                                        }
                                    } else {
block_64:
                                        if ((temp_v1_763 = 0x08002000, var_v0_669 = 0x2000, temp_v1_763 &= temp_a0_297) == var_v0_669) {
                                            temp_v0_329 = (*(u8 *)((s8 *)(temp_s1_34) + (0x92)));
                                            if (temp_v0_329 == 1) {
                                                temp_v0_332 = func_00128AD0(temp_s1_34);
                                                if ((temp_v0_332 != -1) && ((*(s32 *)((s8 *)(temp_s1_34) + (0x84))) != temp_v0_332)) {
                                                    func_00128BF4(temp_s1_34, temp_v0_332, 1);
                                                } else if ((absentIndex = -1, (~(*(s32 *)((s8 *)(temp_s1_34) + (0x84))) != 0) & (~temp_v0_332 == 0))) {
                                                    func_001072B8(temp_s1_34);
                                                    (*(u8 *)((s8 *)(temp_s1_34) + (0x91))) = 0U;
                                                    (*(s32 *)((s8 *)(temp_s1_34) + (0x84))) = absentIndex;
                                                }
                                                goto block_87;
                                            }
                                            if (temp_v0_329 == 2) {
                                                if ((*(u8 *)((s8 *)(temp_s1_34) + (0x20))) == 0) {
                                                    func_00128DDC(temp_s1_34);
                                                } else {
                                                    func_00125F84(temp_s1_34, arg0, &locals.sp28);
                                                    if ((*(u8 *)((s8 *)(temp_s1_34) + (0x20))) == 0) {
                                                        (*(s32 *)((s8 *)(temp_s1_34) + (0))) = (s32) ((*(s32 *)((s8 *)(temp_s1_34) + (0))) & ~0x2000);
                                                    }
                                                    temp_v0_381 = (*(s32 *)((s8 *)(temp_s1_34) + (0x88))) - 1;
                                                    (*(s32 *)((s8 *)(temp_s1_34) + (0x88))) = temp_v0_381;
                                                    if (temp_v0_381 <= 0) {
                                                        (*(s32 *)((s8 *)(temp_s1_34) + (0x88))) = 0x5A;
                                                        if (func_00128050(temp_s1_34) != negativeOne) {
                                                            func_001072B8(temp_s1_34);
                                                            (*(u8 *)((s8 *)(temp_s1_34) + (0x20))) = 0U;
                                                        }
                                                    } else if (func_00128050(temp_s1_34) != negativeOne) {
                                                        (*(s32 *)((s8 *)(temp_s1_34) + (0x88))) = (s32) ((*(s32 *)((s8 *)(temp_s1_34) + (0x88))) - 2);
                                                    }
                                                }
                                            }
                                        } else if (temp_a0_297 & 2) {
                                            temp_v0_410 = (*(u8 *)((s8 *)(temp_s1_34) + (0x91)));
                                            if (temp_v0_410 == 1) {
                                                func_00127EAC(temp_s1_34);
                                            } else if (temp_v0_410 == 2) {
                                                func_00128980(temp_s1_34);
                                            }
block_87:
                                            func_00125F84(temp_s1_34, arg0, &locals.sp28);
                                        } else {
                                            temp_v0_431 = (*(u8 *)((s8 *)(temp_s1_34) + (0x92)));

                                            if (temp_v0_431 == 1) {
                                                temp_f0_434 = *(f32 *)(temp_s1_34 + 0x9C);
                                                if (temp_f0_434 == negativeOne) {
                                                    temp_v0_439 = func_00128AD0(temp_s1_34);
                                                    if (temp_v0_439 != -1) {
                                                        func_00128BF4(temp_s1_34, temp_v0_439, 0);
                                                    }
                                                } else {
                                                    currentAngle = *(f32 *)(temp_s1_34 + 0x18);
                                                    if (currentAngle != temp_f0_434) goto block_100;
                                                    goto block_101;
                                                }
                                            } else if (temp_v0_431 == 2) {
                                                temp_f0_434 = *(f32 *)(temp_s1_34 + 0x9C);
                                                if (temp_f0_434 == negativeOne) {
                                                    func_00128D50(temp_s1_34);
                                                } else {
                                                    currentAngle = *(f32 *)(temp_s1_34 + 0x18);
                                                    if (currentAngle == temp_f0_434) goto block_101;
block_100:
                                                    func_00106CE0(temp_s1_34 + 0x18, temp_s1_34 + 0x9C, 1.0f);
                                                    goto block_154;
block_101:
                                                    temp_v0_483 = *(u8 *)(temp_s1_34 + 0x98);
                                                    *(f32 *)(temp_s1_34 + 0x1C) = currentAngle;
                                                    *(f32 *)(temp_s1_34 + 0x9C) = negativeOne;
                                                    *(u8 *)(temp_s1_34 + 0x98) = 0;
                                                    *(u8 *)(temp_s1_34 + 0x91) = temp_v0_483;
                                                }
                                            } else {
                                                if (temp_a0_297 & 0x800000) {
                                                    if (((*(f32 *)((s8 *)(temp_s1_34) + (8))) == (*(f32 *)((s8 *)(temp_s1_34) + (0x4C)))) && ((*(f32 *)((s8 *)(temp_s1_34) + (0x10))) == (*(f32 *)((s8 *)(temp_s1_34) + (0x54))))) {
                                                        if ((*(f32 *)((s8 *)(temp_s1_34) + (0xC))) != (*(f32 *)((s8 *)(temp_s1_34) + (0x50)))) {
                                                            goto block_115;
                                                        }
                                                        goto block_111;
                                                    }
                                                    goto block_115;
                                                }
                                                temp_a0_514 = (*(u8 *)((s8 *)(temp_s1_34) + (4)));
                                                var_v1_517 = 0;
                                                if (temp_a0_514 < 0x1EU) {
loop_108:
                                                    if ((*(s8 *)((s8 *)((var_v1_517 + ((u32)D_801F0000))) + (0x1039))) != temp_a0_514) {
                                                        var_v1_517 += 1;
                                                        if (var_v1_517 < 8) {
                                                            goto loop_108;
                                                        }
                                                    }
                                                    if (var_v1_517 == 8) {
block_111:
                                                        func_0010766C(temp_s1_34);
                                                    }
                                                } else if (((*(f32 *)((s8 *)(temp_s1_34) + (8))) != (*(f32 *)((s8 *)(temp_s1_34) + (0x4C)))) || ((*(f32 *)((s8 *)(temp_s1_34) + (0x10))) != (*(f32 *)((s8 *)(temp_s1_34) + (0x54)))) || ((*(f32 *)((s8 *)(temp_s1_34) + (0xC))) != (*(f32 *)((s8 *)(temp_s1_34) + (0x50))))) {
block_115:
                                                    (*(u8 *)((s8 *)(temp_s1_34) + (0x20))) = 1U;
                                                    (*(s32 *)((s8 *)(temp_s1_34) + (0x24))) = 1;
                                                    (*(s32 *)((s8 *)(temp_s1_34) + (0))) = (s32) ((*(s32 *)((s8 *)(temp_s1_34) + (0))) | 2);
                                                    (*(f32 *)((s8 *)(temp_s1_34) + (0x28))) = (f32) (*(f32 *)((s8 *)(temp_s1_34) + (0x4C)));
                                                    (*(f32 *)((s8 *)(temp_s1_34) + (0x2C))) = (f32) (*(f32 *)((s8 *)(temp_s1_34) + (0x50)));
                                                    (*(f32 *)((s8 *)(temp_s1_34) + (0x30))) = (f32) (*(f32 *)((s8 *)(temp_s1_34) + (0x54)));
                                                    (*(s32 *)((s8 *)(temp_s1_34) + (0x58))) = (s32) (*(s32 *)((s8 *)(temp_s1_34) + (0x64)));
                                                }

                                            }
                                        }
                                        goto block_154;
                                    }
                                }
                            } else {
                                if ((temp_a0_260 & 0x11) == 0x11) {
                                    if (!(temp_a0_260 & 0x80)) goto block_154;
                                    if (func_0012DB2C(temp_s1_34) == 0) {
                                        if ((*(u8 *)((s8 *)(((func_0012E968(temp_s1_34) * 0xB) + ((u32)D_801969BA))) + (7))) != 7) {
                                            temp_a0_592 = (*(s32 *)((s8 *)(temp_s1_34) + (0)));
                                            if ((temp_a0_592 & 0x9000) != 0x8000) {
                                                if ((*(u8 *)(temp_s1_34 + 0x92) == 1) && !(temp_a0_592 & 0x1002)) {
                                                    (*(s32 *)((s8 *)(temp_s1_34) + (0x24))) = 0;
                                                    (*(u8 *)((s8 *)(temp_s1_34) + (0x20))) = 0U;
                                                    if ((*(s32 *)((s8 *)(temp_s1_34) + (0xA8))) != 0) {
                                                        if (func_00127D30(temp_s1_34) != -1) {
                                                            func_00120CF0(temp_s1_34, (*(f32 *)((s8 *)(temp_s1_34) + (8))), (*(f32 *)((s8 *)(temp_s1_34) + (0xC))), (*(f32 *)((s8 *)(temp_s1_34) + (0x10))));
                                                            (*(u8 *)((s8 *)(temp_s1_34) + (0x92))) = 1U;
                                                            (*(u8 *)((s8 *)(temp_s1_34) + (0x91))) = 1U;
                                                            func_00127EAC(temp_s1_34);
                                                        } else {
                                                            (*(u8 *)((s8 *)(temp_s1_34) + (0x91))) = 0U;
                                                        }
                                                    } else {
                                                        (*(u8 *)((s8 *)(temp_s1_34) + (0x91))) = 0U;
                                                    }
                                                } else {
                                                    if ((temp_v1_763 = 0x801002, var_v0_669 = 0x800000, temp_v1_763 &= temp_a0_592) == var_v0_669) {
                                                        if (((*(f32 *)((s8 *)(temp_s1_34) + (8))) != (*(f32 *)((s8 *)(temp_s1_34) + (0x4C)))) || ((*(f32 *)((s8 *)(temp_s1_34) + (0x10))) != (*(f32 *)((s8 *)(temp_s1_34) + (0x54))))) {
                                                            func_001072B8(temp_s1_34);
                                                            do {
                                                            do {
                                                            copiedX = *(f32 *)(temp_s1_34 + 0x4C);
                                                            copiedY = *(f32 *)(temp_s1_34 + 0x50);
                                                            copiedZ = *(f32 *)(temp_s1_34 + 0x54);
                                                            } while (0);
                                                            copiedField64 = *(s32 *)(temp_s1_34 + 0x64);
                                                            temp_v1_763 = *(s32 *)temp_s1_34;
                                                            *(s32 *)(temp_s1_34 + 0x24) = 1;
                                                            *(u8 *)(temp_s1_34 + 0x20) = 1;
                                                            *(u8 *)(temp_s1_34 + 0x92) = 0;
                                                            *(u8 *)(temp_s1_34 + 0x91) = 0;
                                                            *(s32 *)(temp_s1_34 + 0x58) = copiedField64;
                                                            } while (0);
                                                            copiedField64 = ~4;
                                                            temp_v1_763 &= copiedField64;
                                                            copiedField64 = ~0x200;
                                                            temp_v1_763 &= copiedField64;
                                                            temp_v1_763 |= 2;
                                                            *(f32 *)(temp_s1_34 + 0x28) = copiedX;
                                                            *(f32 *)(temp_s1_34 + 0x2C) = copiedY;
                                                            *(f32 *)(temp_s1_34 + 0x30) = copiedZ;
                                                            *(s32 *)temp_s1_34 = temp_v1_763;
                                                        } else {
                                                            var_v0_669 = temp_a0_592 & 0x40000;
                                                            goto block_142;
                                                        }
                                                    } else if (((*(u8 *)((s8 *)(temp_s1_34) + (0x91))) == 1) && ((temp_a0_592 & 0x1002) == 2)) {
                                                        func_00127EAC(temp_s1_34);
                                                    } else {
                                                        temp_v1_763 = 0x41000;
                                                        var_v0_669 = *(s32 *)temp_s1_34 & temp_v1_763;
block_142:
                                                        if ((var_v0_669 == 0) && (func_0012E8EC(temp_s1_34) == 1) && ((u8) (*(u8 *)((s8 *)(temp_s1_34) + (4))) < 0x1EU)) {
                                                            func_0010766C(temp_s1_34);
                                                        }
                                                    }
                                                }
                                                temp_s0_708 = (*(u8 *)((s8 *)(((func_0012E968(temp_s1_34) * 0xB) + ((u32)D_80190000))) + (0x69C1)));
                                                func_0012D170(temp_s1_34, temp_s0_708, &locals.sp28, func_0012E8EC(temp_s1_34));
                                            }
                                            func_0012BC64(temp_s1_34 + 8, (func_0012E968(temp_s1_34) * 0xB) + ((u32)D_801969B8), &locals.sp28, 1);
                                        } else {
                                            var_s0_579 = 0;
                                            goto block_149;
                                        }
                                    } else {
                                        var_s0_579 = 0;
block_149:
                                        for (var_s0_579 = 0,
                                             memberBase = D_801969BA,
                                             memberClearMask = 0xFFFBFFFF,
                                             memberTestMask = 0x20000,
                                             memberSetMask = 0x40000;
                                             var_s0_579 < 5; var_s0_579++) {
                                            memberOffset = func_0012E968(temp_s1_34) * 0xB;
                                            memberOffset += (u32)memberBase;
                                            memberOffset += var_s0_579;
                                            temp_a0_750 = *(u8 *)memberOffset;
                                            if (temp_a0_750 != 0xFF) {
                                                temp_a0_754 = temp_a0_750 * 4;
                                                temp_v1_758 = (s32 *)mainUnits[temp_a0_750];
                                                *temp_v1_758 &= memberClearMask;
                                                temp_a0_762 = (s32 *)mainUnits[temp_a0_750];
                                                temp_v1_763 = *temp_a0_762;
                                                if (temp_v1_763 & memberTestMask) {
                                                    *temp_a0_762 = temp_v1_763 | memberSetMask;
                                                }
                                            }
                                        }
                                    }
                                }
block_154:
                                mapZ = *(f32 *)(temp_s1_34 + 0x10);
                                temp_f2_776 = *(f32 *)D_801F0D9C;
                                mapZ -= temp_f2_776;
                                mapZ *= 64.0f;
                                mapDenominator = *(f32 *)D_801F0DA4 - temp_f2_776;
                                mapZ /= mapDenominator;
                                temp_f6_786 = *(f32 *)D_801F0D98;
                                mapX = *(f32 *)(temp_s1_34 + 8);
                                mapX -= temp_f6_786;
                                mapX *= 64.0f;
                                mapDenominator = *(f32 *)D_801F0DA0 - temp_f6_786;
                                mapX /= mapDenominator;
                                *(s32 *)(temp_s1_34 + 0x14) = ((s32)mapZ << 6) + (s32)mapX;
                                if ((locals.sp37 == 0) && (func_00131480(temp_s1_34, 0x1C) != 0)) {
                                    var_s3_13 -= 1;
                                    locals.sp37 = tailOne;
                                } else {
                                    locals.sp37 = 0;
                                }
                            }
                        }
                    }
                }
            }
            var_s3_13 += 1;
        } while (var_s3_13 < *(s32 *)((u32)D_801F367C));
        var_s3_13 = 0;
    }
    lateUnits = (void **)D_801F0CB0;
    lateMask = 0x20000;
    lateTwo = 2;
    lateOne = 1;
    var_s0_831 = (s8 *)((u32)D_801969BF);
    var_s1_832 = 0;
    do {
        if ((*(u8 *)((s8 *)((var_s1_832 + ((u32)D_80190000))) + (0x69B9))) & 1) {
            temp_v1_847 = *(s32 *)lateUnits[func_0012E950(*(u8 *)(var_s1_832 + (u32)D_80190000 + 0x69BA))];
            if (temp_v1_847 & lateMask) {
                *var_s0_831 = lateTwo;
            } else if (temp_v1_847 & 0x1002) {
                *var_s0_831 = lateOne;
            } else {
                *var_s0_831 = 0;
            }
        }
        var_s0_831 += 0xB;
        var_s3_13 += 1;
        var_s1_832 += 0xB;
    } while (var_s3_13 < 0xA);
    if (*(u8 *)((u32)D_8018F481) == 0x36) {
        func_0005c090(0x14, 1);
        if (func_0005c110(0x14) != 0) {
            if (func_0005c110(0x1B) == 0) {
                var_s3_13 = 0x20;
                var_v1_880 = (void **)((u32)D_801F0D30);
loop_173:
                temp_s1_882 = *var_v1_880;
                if ((*(s32 *)((s8 *)(temp_s1_882) + (0))) & 0x11) {
                    var_s3_13 += 1;
                    var_v1_880 += 1;
                    if (var_s3_13 < 0x31) goto loop_173;
                }
                if (var_s3_13 < 0x31) {
                    if (func_0005c110(0x15) != 0) {
                        var_s2_898 = 0;
                    } else if (func_0005c110(0x16) != 0) {
                        var_s2_898 = 1;
                    } else {
                        var_s2_898 = ((0 - (func_0005c110(0x17) == 0)) & 3) | 2;
                    }
                    os_inval_icache(((u32)D_80214F80), ((u32)D_802172B0) - ((u32)D_80214F80));
                    os_inval_dcache(((u32)D_802172B0_data), ((u32)D_80217350) - ((u32)D_802172B0_data));
                    func_0002de50((u32)D_00195410, ((u32)D_80214F80_dma), (u32)D_001977E0 - (u32)D_00195410);
                    if (((u32)D_80217350_end) != ((u32)D_80217350_start)) {
                        memset_00023780(((u32)D_80217350_start), ((u32)D_80217350_end) - ((u32)D_80217350_start));
                    }
                    var_v0_935 = var_s2_898 & 0xFF;
                    temp_s0_940 = var_v0_935 * 8;
                    func_001957D0((var_v0_935 * 0x23) + ((u32)D_801E85A0), var_s3_13 & 0xFF);
                    temp_v0_955 = var_s3_13 * 0x19;
                    (*(u8 *)((s8 *)((temp_v0_955 + ((u32)D_80190000))) + (0x71FC))) = (u8) (*(u8 *)((s8 *)((temp_s0_940 + ((u32)D_801F0000))) + (-0x7A80)));
                    (*(u8 *)((s8 *)((temp_v0_955 + ((u32)D_80190000))) + (0x7207))) = (u8) (*(u8 *)((s8 *)((temp_s0_940 + ((u32)D_801F0000))) + (-0x7A7F)));
                    func_0012967C(var_s3_13);
                    (*(u8 *)((s8 *)(temp_s1_882) + (0xBA))) = (u8) (*(u8 *)((s8 *)((temp_s0_940 + ((u32)D_801F0000))) + (-0x7A7E)));
                    (*(u8 *)((s8 *)(temp_s1_882) + (0xBB))) = (u8) (*(u8 *)((s8 *)((temp_s0_940 + ((u32)D_801F0000))) + (-0x7A7D)));
                    rangeX = *(f32 *)D_801F0DA0;
                    temp_f4_978 = *(f32 *)D_801F0D98;
                    rangeX -= temp_f4_978;
                    rangeX *= (f32)*(u8 *)(temp_s0_940 + (u32)D_801F0000 - 0x7A7C);
                    rangeX /= 256.0f;
                    temp_f8_993 = temp_f4_978 + rangeX;
                    rangeZ = *(f32 *)D_801F0DA4;
                    temp_f4_995 = *(f32 *)D_801F0D9C;
                    rangeZ -= temp_f4_995;
                    locals.position[0] = temp_f8_993;
                    rangeZ *= (f32)*(u8 *)(temp_s0_940 + (u32)D_801F0000 - 0x7A7B);
                    rangeZ /= 256.0f;
                    temp_f4_1006 = temp_f4_995 + rangeZ;
                    locals.position[2] = temp_f4_1006;
                    recordStride = 8;
                    if ((*(s32 *)((s8 *)(temp_s1_882) + (0x70))) == 1) {
                        locals.position[1] = func_000F3428(*(u8 *)((u32)D_800E7AB9), temp_f8_993, temp_f4_1006);
                    } else {
                        locals.position[1] = func_000F315C(*(u8 *)((u32)D_800E7AB9), temp_f8_993, temp_f4_1006);
                    }
                    func_801CC480(temp_s1_882, locals.position[0], locals.position[1], locals.position[2], 0);
                    (*(s32 *)((s8 *)(temp_s1_882) + (0))) = (s32) ((*(s32 *)((s8 *)(temp_s1_882) + (0))) & ~1);
                    temp_v0_1044 = var_s3_13 * 0x19;
                    (*(s8 *)((s8 *)((temp_v0_1044 + ((u32)D_80190000))) + (0x71F1))) = (s8) ((*(u8 *)((s8 *)((temp_v0_1044 + ((u32)D_80190000))) + (0x71F1))) & 0xFB);
                    temp_v1_1054 = (*(u8 *)((s8 *)(((recordStride * var_s2_898) + ((u32)D_801F0000))) + (-0x7A7A)));
                    if (temp_v1_1054 >= 3U) {
                        var_f0_1068 = ((f32) (temp_v1_1054 - 1) * 0.125f) - 0.25f;
                    } else {
                        var_f0_1068 = ((f32) (temp_v1_1054 - 1) * 0.125f) + 0.75f;
                    }
                    (*(f32 *)((s8 *)(temp_s1_882) + (0x18))) = var_f0_1068;
                    (*(f32 *)((s8 *)(temp_s1_882) + (0x1C))) = var_f0_1068;
                    func_00195D9C(var_s3_13 & 0xFF, 0, 0, 0);
                    (*(s16 *)((s8 *)(((var_s3_13 * 2) + ((u32)D_80190000))) + (0x367C))) = (s16) (*(u8 *)((s8 *)(((recordStride * var_s2_898) + ((u32)D_801F0000))) + (-0x7A79)));
                    func_00128E80();
                    func_0005c090(0x1B, 1);
                }
            }
        }
    }
}
