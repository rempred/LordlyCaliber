#include "common/types.h"

typedef float f32;
typedef s32 UnknownWord;

typedef struct { s32 words[20]; } LocalCopy;
/* Retail clears six words beside the 80-byte work copy. The sentinel walk
 * retains that layout; a bound on all runtime program lengths is not inferred. */
typedef struct { LocalCopy copy; s32 buffer[6]; } LocalData;
s32 func_00022e60();
s32 func_00043ad8(u8, u8);
u8 func_000454e0(s32);
void func_001F0E64(LocalCopy *, s32);
s32 func_001F197C(s32 *, LocalCopy *, s32 *);
s32 func_00201584(s32, s32);
s32 func_00201798(s32, s32, s32, UnknownWord);
s32 func_0020BFE4();
s32 func_0020BFF8(s32 *);
s32 func_0020C014(s32 *);
s32 func_0020C104(s32 *);
s32 func_0020C164(s32 *);
s32 func_0020C2C0(s32 *);
s32 func_0020C32C(s32 *);
s32 func_0020C448(s32 *);
s32 *func_0020C478(s32);
s32 func_0020D72C(s32 *);
void func_0021D200(UnknownWord, s32, s32 *);
void func_0021D230(UnknownWord, s32, s32 *, u32);
void func_0021D25C(UnknownWord, s32, s32 *, UnknownWord, s32);
void func_0021D28C(UnknownWord, s32, s32 *, UnknownWord, s32, s32);
void func_0021D2C0(UnknownWord, s32, UnknownWord, UnknownWord, s32, s32, s32);
s32 func_0022222C(s32 *, s32 *, s32, s32);
s32 func_002223E0(s32 *, s32 *, u8, u8, s32, s32);
s32 func_0022257C(s32, UnknownWord, UnknownWord);
s32 func_0022A4E0(s32 *);
UnknownWord func_0022ADFC(s32 *, UnknownWord, s32, s32, s32, s32, s32, s32);
u32 func_00233210(s32 *, s32 *);
s32 func_00233ED4(s32 *);
UnknownWord func_0023431C(s32 *, s32, u8);
s32 func_00235754(s32 *, s32 *);
s32 func_002361EC(s32 *);
s32 func_002363FC(s32 *);
u32 func_002365BC(s32 *, s32 *, s32, s32);
s32 func_0023697C(s32 *, s32 *, f32, UnknownWord, s32, s32);
s32 func_00237750(s32 *, s32 *, UnknownWord, UnknownWord, s32);
s32 func_00237810(s32 *, s32 *);
s32 func_00237890(s32 *, s32 *);
u8 func_00237AF8(s32 *, UnknownWord);
UnknownWord memset_00023780(s32 *, UnknownWord);
u32 rand(void);
s32 func_0022B1F4(s32 *sourceWords, s32 *outWord) {
    LocalData data;
    u32 actionByte;
    s32 *last;
    s32 missMask;
    u32 earlyLoaded, defaultLoaded, loaded;
    s32 *outputWord;
    s32 entryCount;
    s32 sp9C;
    s32 spA4;
    s32 spAC;
    s32 spB4;
    u8 spBF;
    u8 spC7;
    u8 spCF;
    u8 spD7;
    s32 spDC;
    s32 spE4;
    u8 *var_v1_366;
    UnknownWord var_a3_341;
    UnknownWord var_s1_507;
    s32 *temp_v0_169;
    s32 temp_a0_203;
    s32 temp_a0_37;
    s32 temp_a0_446;
    s32 temp_a1_889;
    s32 temp_s0_125;
    s32 temp_s0_294;
    u32 temp_s0_303;
    s32 temp_s0_388;
    s32 temp_s0_459;
    s32 temp_s0_568;
    s32 temp_s0_754;
    s32 temp_s0_846;
    s32 temp_s0_856;
    s32 temp_s1_280;
    u32 temp_s1_305;
    s32 temp_s1_815;
    s32 temp_s6_230;
    s32 temp_s7_636;
    s32 temp_t1_440;
    s32 temp_v0_104;
    s32 temp_v0_257;
    s32 temp_v0_31;
    s32 temp_v0_352;
    s32 temp_v0_360;
    s32 temp_v0_854;
    s32 temp_v0_951;
    s32 temp_v1_665;
    s32 temp_v1_803;
    s32 temp_v1_895;
    s32 temp_v1_946;
    s32 var_a0_58;
    s32 var_a1_496;
    s32 var_a1_872;
    /* The three-record walk and buffer walk reuse this real index. */
    s32 index;
    s32 var_s5_548;
    s32 var_s8_30;
    s32 var_v0_63;
    s32 var_v1_193;
    s32 var_v1_36;
    s32 recordWord6C;
    s32 var_v1_491;
    s32 var_v1_69;
    s32 var_v1_742;
    u32 temp_s0_575;
    u32 temp_s3_301;
    u32 var_a1_219;
    u32 var_a1_47;
    u32 var_a1_87;
    u8 temp_s0_28;
    u32 temp_v0_771;
    outputWord = outWord;
    spD7 = 0;
    temp_s0_28 = (*(u8 *)((s8 *)((((*(s32 *)((s8 *)(sourceWords) + (0x7C))) * 0x10) + 0x80190000)) + (-0x557E)));
    memset_00023780(&data.buffer[0], 0x18);
    var_s8_30 = 0;
    /* Zero before use, after buffer/offset initialization. This order retains
     * the original entry stores under the pinned compiler. */
    spA4 = 0;
    temp_v0_31 = func_00233ED4(sourceWords);
    (*(s32 *)((s8 *)(sourceWords) + (0x68))) = temp_v0_31;
    if (temp_v0_31 < 0) {
        var_v1_36 = (*(s32 *)((s8 *)(sourceWords) + (0x94)));
        temp_a0_37 = *outputWord;
        if (var_v1_36 < temp_a0_37) {
            var_v1_36 = temp_a0_37;
        }
        earlyLoaded = *(u32 *)0x801CE8C0;
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = var_v1_36;
        earlyLoaded = *(u32 *)(earlyLoaded + 0x828);
        if (earlyLoaded < (u32) var_v1_36) {
            var_a1_47 = (u32) var_v1_36;
        } else {
            var_a1_47 = earlyLoaded;
        }
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = (s32) var_a1_47;
        func_0021D200(0x1C, (s32) var_a1_47, sourceWords);
        recordWord6C = (*(s32 *)((s8 *)(sourceWords) + (0x6C)));
        var_a0_58 = (*(s32 *)((s8 *)(sourceWords) + (0x94)));
        recordWord6C += 1;
        (*(s32 *)((s8 *)(sourceWords) + (0x6C))) = recordWord6C;
        *outputWord = var_a0_58;
        return 0;
    } else if (temp_s0_28 == 0) {
        temp_s6_230 = *outputWord;
        var_v1_69 = (*(s32 *)((s8 *)(sourceWords) + (0x94)));
        temp_s6_230 = temp_s6_230 & ((s32) ~temp_s6_230 >> 0x1F);
        if (temp_s6_230 < var_v1_69) {
            temp_s6_230 = var_v1_69;
        }
        if (var_v1_69 < temp_s6_230) {
            var_v1_69 = temp_s6_230;
        }
        defaultLoaded = *(u32 *)0x801CE8C0;
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = var_v1_69;
        defaultLoaded = *(u32 *)(defaultLoaded + 0x828);
        if (defaultLoaded < (u32) var_v1_69) {
            var_a1_87 = (u32) var_v1_69;
        } else {
            var_a1_87 = defaultLoaded;
        }
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = (s32) var_a1_87;
        func_0021D200(0x1C, (s32) var_a1_87, sourceWords);
        temp_s6_230 = (*(s32 *)((s8 *)(sourceWords) + (0x94)));
        (*(s32 *)((s8 *)(sourceWords) + (0x6C))) = (s32) ((*(s32 *)((s8 *)(sourceWords) + (0x6C))) + 1);
        if (func_0020BFE4() != 0) {
            temp_v0_104 = (*(s32 *)((s8 *)(sourceWords) + (0)));
            data.copy = *(LocalCopy *)(temp_v0_104 + 0x44);
            temp_s0_125 = func_00201798((*(s32 *)((s8 *)(sourceWords) + (0x48))), (*(s32 *)((s8 *)(sourceWords) + (0x4C))), (*(s32 *)((s8 *)(sourceWords) + (0x7C))), 0) & 0xFF;
            func_001F0E64(&data.copy, temp_s0_125);
            sp9C = func_001F197C(sourceWords, &data.copy, 0);
            func_0021D25C(0x1D, temp_s6_230, sourceWords, 0xFF, temp_s0_125);
        } else {
            sp9C = 0xA;
        }
        if (func_0020BFE4() == 0) {
            func_0021D28C(0x36, temp_s6_230, sourceWords, 0xE, 0, (*(s32 *)((s8 *)(sourceWords) + (0x7C))));
        }
        (*(s32 *)((s8 *)(sourceWords) + (0x88))) = sp9C;
        (*(s32 *)((s8 *)(sourceWords) + (0x84))) = 0;
        (*(s32 *)((s8 *)(sourceWords) + (0x8C))) = 0;
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = (s32) (temp_s6_230 + sp9C);
        var_a0_58 = temp_s6_230 + (*(s32 *)((s8 *)(sourceWords) + (0x88)));
        *outputWord = var_a0_58;
        return 1;
    } else {
        temp_v0_169 = func_0020C478(temp_v0_31);
        spC7 = (*(u8 *)((s8 *)(temp_v0_169) + (0x57)));
        spCF = (*(u8 *)((s8 *)(temp_v0_169) + (0x5B)));
        if (func_0020BFE4() != 0) {
            var_s8_30 = func_0022222C(sourceWords, temp_v0_169, (*(s32 *)((s8 *)(sourceWords) + (0x5C))), (*(s32 *)((s8 *)(sourceWords) + (0x64))));
            if (func_0020C164(sourceWords) != 0) {
                var_s8_30 -= 0xF;
            }
        }
        var_v1_193 = (*(s32 *)((s8 *)(sourceWords) + (0x94)));
        temp_s6_230 = *outputWord - var_s8_30;
        temp_s6_230 = temp_s6_230 & ((s32) ~temp_s6_230 >> 0x1F);
        if (temp_s6_230 < var_v1_193) {
            temp_s6_230 = var_v1_193;
        }
        temp_a0_203 = (*(s32 *)((s8 *)(temp_v0_169) + (0x94)));
        if ((temp_s6_230 + var_s8_30) < temp_a0_203) {
            temp_s6_230 = temp_a0_203 - var_s8_30;
        }
        if (var_v1_193 < temp_s6_230) {
            var_v1_193 = temp_s6_230;
        }
        loaded = *(u32 *)0x801CE8C0;
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = var_v1_193;
        loaded = *(u32 *)(loaded + 0x828);
        if (loaded < (u32) var_v1_193) {
            var_a1_219 = (u32) var_v1_193;
        } else {
            var_a1_219 = loaded;
        }
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = (s32) var_a1_219;
        func_0021D200(0x1C, (s32) var_a1_219, sourceWords);
        temp_s6_230 = (*(s32 *)((s8 *)(sourceWords) + (0x94)));
        (*(s32 *)((s8 *)(sourceWords) + (0x6C))) = (s32) ((*(s32 *)((s8 *)(sourceWords) + (0x6C))) + 1);
        if (func_0020BFE4() != 0) {
            if (func_0020C104(sourceWords) != 0) {
                func_0021D28C(3, temp_s6_230, sourceWords, 0xFF, (*(s32 *)((s8 *)(sourceWords) + (0x68))), 5);
            } else {
                func_0021D28C(2, temp_s6_230, sourceWords, 0xFF, (*(s32 *)((s8 *)(sourceWords) + (0x68))), 5);
            }
            if ((func_00043ad8((*(u8 *)((s8 *)(sourceWords) + (0x4B))), (*(u8 *)((s8 *)(sourceWords) + (0x4F)))) & 0xFF) != 1) {
                temp_v0_257 = (*(s32 *)((s8 *)(sourceWords) + (0)));
                data.copy = *(LocalCopy *)(temp_v0_257 + 0x44);
                func_001F0E64(&data.copy, 6);
                temp_s1_280 = func_001F197C(sourceWords, &data.copy, 0);
                func_0021D25C(5, temp_s6_230 + var_s8_30, sourceWords, 0xFF, 6);
                var_s8_30 += temp_s1_280;
            }
        }
        temp_s0_294 = func_002361EC(sourceWords);
        temp_s3_301 = func_002365BC(sourceWords, temp_v0_169, temp_s0_294, func_002363FC(temp_v0_169));
        temp_s0_303 = rand();
        temp_s1_305 = rand();
        temp_s3_301 = (u32) ((((temp_s0_303 << 0x12) & 0x0C000000) | (temp_s1_305 << 0xF) | rand()) % 100) < temp_s3_301;
        data.copy = *(LocalCopy *)(*sourceWords + 0x44);
        var_a3_341 = 0;
        if (temp_s3_301 == 0) {
            if (func_00201584((*(s32 *)((s8 *)(temp_v0_169) + (0x48))), (*(s32 *)((s8 *)(temp_v0_169) + (0x4C)))) & 0xFF) {
                var_a3_341 = 2;
            } else {
                var_a3_341 = 1;
            }
        }
        temp_v0_352 = func_00201798((*(s32 *)((s8 *)(sourceWords) + (0x48))), (*(s32 *)((s8 *)(sourceWords) + (0x4C))), (*(s32 *)((s8 *)(sourceWords) + (0x7C))), var_a3_341);
        func_001F0E64(&data.copy, temp_v0_352 & 0xFF);
        temp_v0_360 = func_001F197C(sourceWords, &data.copy, &data.buffer[0]);
        entryCount = 0;
        sp9C = temp_v0_360;
        if (data.buffer[0] >= 0) {
            var_v1_366 = (u8 *)&data;
            do {
                var_v1_366 += 4;
                entryCount += 1;
            } while ((*(s32 *)((s8 *)(var_v1_366) + (0x50))) >= 0);
        }
        if (func_0020BFE4() != 0) {
            if (func_0020C164(sourceWords) != 0) {
                func_0021D200(0x22, temp_s6_230 + var_s8_30, sourceWords);
                var_s8_30 += 5;
                temp_s0_388 = temp_s6_230 + var_s8_30;
                func_0021D28C(0x21, temp_s0_388, sourceWords, 0xFF, temp_v0_352 & 0xFF, (*(s32 *)((s8 *)(sourceWords) + (0x68))));
                func_0021D25C(5, temp_s0_388 + data.buffer[0], sourceWords, 0xFF, 0xFF);
            } else {
                if (temp_s3_301 != 0) {
                    func_0021D28C(6, temp_s6_230 + var_s8_30, sourceWords, 0xFF, temp_v0_352 & 0xFF, (*(s32 *)((s8 *)(sourceWords) + (0x7C))));
                } else {
                    func_0021D28C(7, temp_s6_230 + var_s8_30, sourceWords, 0xFF, temp_v0_352 & 0xFF, (*(s32 *)((s8 *)(sourceWords) + (0x7C))));
                }
            }
        } else {
            if (temp_s3_301 != 0) {
                func_0021D28C(6, temp_s6_230 + var_s8_30, sourceWords, 0, 0, (*(s32 *)((s8 *)(sourceWords) + (0x7C))));
            } else {
                func_0021D28C(7, temp_s6_230 + var_s8_30, sourceWords, 0, 0, (*(s32 *)((s8 *)(sourceWords) + (0x7C))));
            }
            sp9C = 0xF;
            data.buffer[0] = 0xA;
        }
        temp_t1_440 = temp_s6_230 + var_s8_30;
        spAC = temp_t1_440;
        spB4 = temp_t1_440 + sp9C;
        temp_a0_446 = func_0020C448(sourceWords) & 0xFFFF;
        if (temp_a0_446 != 0) {
            spBF = func_000454e0(temp_a0_446);
        } else {
            spBF = 0;
        }
        temp_s0_459 = temp_s6_230 + var_s8_30 + data.buffer[entryCount - 1];
        if (temp_s3_301 != 0) {
            func_0021D230(0x3E, temp_s0_459, sourceWords, spBF);
            if (func_0020BFE4() != 0) {
                func_0021D2C0(0x17, temp_s0_459, 0, 0x5D, (s32) spBF, func_0020D72C(sourceWords), (*(s32 *)((s8 *)(sourceWords) + (0x68))));
            }
            if (temp_s0_459 < spAC) {
                spAC = temp_s0_459;
            }
            if (func_0020BFE4() != 0) {
                var_v1_491 = 0x19;
            } else {
                var_v1_491 = 5;
            }
            var_a1_496 = var_v1_491 + temp_s0_459;
            if (spB4 < var_a1_496) {
                spB4 = var_a1_496;
            }
        } else if (func_00201584((*(s32 *)((s8 *)(temp_v0_169) + (0x48))), (*(s32 *)((s8 *)(temp_v0_169) + (0x4C)))) & 0xFF) {
            if (func_0020BFF8(temp_v0_169) != 0) {
                var_s1_507 = 0x60;
            } else {
                var_s1_507 = 0x5F;
            }
            if (func_0020BFE4() != 0) {
                func_0021D2C0(0x17, temp_s0_459, 0, var_s1_507, (s32) spBF, func_0020D72C(sourceWords), (*(s32 *)((s8 *)(sourceWords) + (0x68))));
            }
            if (temp_s0_459 < spAC) {
                spAC = temp_s0_459;
            }
            missMask = 0xF;
            if (func_0020BFE4() == 0) missMask = 0;
            missMask |= 5;
            var_a1_496 = temp_s0_459 + missMask;
            if (spB4 < var_a1_496) {
                spB4 = var_a1_496;
            }
        }
        var_s5_548 = 0;
        if (temp_s3_301 != 0) {
            spDC = func_00237810(sourceWords, temp_v0_169);
            if (func_0020C104(sourceWords) != 0) {
                index = 0;
                do {
                    if (sourceWords[index] != 0) {
                        temp_s0_568 = func_002361EC(sourceWords);
                        temp_s0_575 = func_002365BC(sourceWords, temp_v0_169, temp_s0_568, func_002363FC(temp_v0_169));
                        var_s5_548 += (func_00022e60() % 100) < (s32) temp_s0_575;
                    }
                    index += 1;
                } while ((u32)index < 3U);
                spE4 = func_0023697C(sourceWords, temp_v0_169, (f32) (s32) ((f32) func_00235754(sourceWords, temp_v0_169) * (*(f32 *)((s8 *)(((var_s5_548 * 4) + 0x801F0000)) + (0x68D0)))), 0, 0, 0);
            } else {
                spE4 = func_00237750(sourceWords, temp_v0_169, 0, 0, 0);
            }
            temp_s7_636 = func_0022257C((*(s32 *)((s8 *)(temp_v0_169) + (0))) + 0x44, 0x11, 0);
            for (index = 0; index < entryCount - 1; index++) {
                if (func_0020C32C(temp_v0_169) != 0) {
                    func_0021D230(0x2D, temp_s6_230 + var_s8_30 + data.buffer[index], temp_v0_169, 0xFFU);
                } else {
                    func_0021D25C(0x1D, temp_s6_230 + var_s8_30 + data.buffer[index], temp_v0_169, 0xFF, 0x11);
                    {
                        s32 nextWord = data.buffer[index + 1];
                        temp_v1_665 = data.buffer[index];
                        if (temp_s7_636 < (nextWord - temp_v1_665)) {
                            func_0021D25C(0x1D, temp_s6_230 + var_s8_30 + temp_v1_665 + temp_s7_636, temp_v0_169, 0xFF, 0x12);
                        }
                    }
                }
            }
            if (spDC != 0) {
                if (func_0020BFE4() != 0) {
                    func_0021D200(0x33, (temp_s6_230 + var_s8_30 + data.buffer[entryCount - 1]) - 0xA, temp_v0_169);
                }
                func_0021D25C(0x34, temp_s6_230 + var_s8_30 + data.buffer[entryCount - 1], temp_v0_169, 0xFF, (s32) spBF);
                spE4 *= 2;
            }
            if (spBF == 2) {
                spD7 = func_00237AF8(temp_v0_169, 2);
            }
            actionByte = spBF;
            {
                /* Keep the actual copy-tail base local to this block. Folding
                 * it into the index expression changes the call delay slots. */
                s32 *copyTailBase = (s32 *)((u8 *)&data + 76);
                last = copyTailBase + entryCount;
            }
            temp_s3_301 = func_00237890(sourceWords, temp_v0_169);
            if (spDC != 0) {
                var_v1_742 = func_0022A4E0(temp_v0_169);
            } else {
                var_v1_742 = 0;
            }
            temp_s0_754 = temp_s6_230 + var_s8_30;
            func_0022ADFC(temp_v0_169, 0, temp_s0_754 + *last, spE4, actionByte, temp_s3_301 & 0xFF, 1, var_v1_742);
            if ((func_0020C2C0(temp_v0_169) != 0) && (func_0020C014(sourceWords) != 0) && (func_0020C104(sourceWords) == 0)) {
                temp_v0_771 = func_00233210(sourceWords, temp_v0_169);
                if (temp_v0_771 != 0) {
                    (*(u8 *)((s8 *)(sourceWords) + (0x34))) = (u8) ((*(u8 *)((s8 *)(sourceWords) + (0x34))) + temp_v0_771);
                    func_0021D230(0x3F, temp_s0_754 + sp9C, sourceWords, temp_v0_771);
                }
            }
            if ((spD7 != 0) && (func_0020C2C0(temp_v0_169) == 0) && (func_0020C32C(temp_v0_169) == 0)) {
                func_0023431C(temp_v0_169, temp_s6_230 + var_s8_30 + data.buffer[entryCount - 1], spD7);
                temp_v1_803 = (*(s32 *)((s8 *)(temp_v0_169) + (0x94)));
                if (spB4 < temp_v1_803) {
                    spB4 = temp_v1_803;
                    goto hit_bound_update;
                }
                goto after_bound_update;
            }
/* Keep the genuine hit and terminal joins separate. The pinned compiler
 * merges their tail after reload allocation; an early goto changes the later
 * spill registers and prevents the publication stores from merging. */
hit_bound_update:
            temp_v1_895 = (*(s32 *)((s8 *)(temp_v0_169) + (0x94)));
            if (spB4 < temp_v1_895) {
                spB4 = temp_v1_895;
            }
            goto after_bound_update;
        } else {
            temp_s1_815 = func_00201584((*(s32 *)((s8 *)(temp_v0_169) + (0x48))), (*(s32 *)((s8 *)(temp_v0_169) + (0x4C)))) & 0xFF;
            data.copy = *(LocalCopy *)(*temp_v0_169 + 0x44);
            func_001F0E64(&data.copy, ((0 - (temp_s1_815 == 0)) & 0x17) | 0x15);
            temp_s3_301 = func_001F197C(temp_v0_169, &data.copy, 0);
            temp_s0_846 = temp_s6_230 + var_s8_30;
            func_0021D230(0xE, temp_s0_846 + data.buffer[0], temp_v0_169, 0xFFU);
            temp_v0_854 = (*(s32 *)((s8 *)(temp_v0_169) + (0)));
            temp_s0_856 = temp_s0_846 + data.buffer[entryCount - 1] + temp_s3_301;
            data.copy = *(LocalCopy *)(temp_v0_854 + 0x44);
            var_a1_872 = 0x18;
            if (temp_s1_815 != 0) {
                var_a1_872 = 0x16;
            }
            func_001F0E64(&data.copy, var_a1_872);
            temp_s1_280 = func_001F197C(temp_v0_169, &data.copy, 0);
            func_0021D230(0xF, temp_s0_856, temp_v0_169, 0xFFU);
            temp_s3_301 += temp_s1_280;
            temp_a1_889 = temp_s0_856 + temp_s3_301;
            if ((*(s32 *)((s8 *)(temp_v0_169) + (0x94))) < temp_a1_889) {
                (*(s32 *)((s8 *)(temp_v0_169) + (0x94))) = temp_a1_889;
            }
block_149:
            temp_v1_895 = (*(s32 *)((s8 *)(temp_v0_169) + (0x94)));
            if (spB4 < temp_v1_895) {
                spB4 = temp_v1_895;
            }
        }
after_bound_update:
        if (func_0020BFE4() != 0) {
            spA4 = func_002223E0(sourceWords, temp_v0_169, spC7, spCF, (*(s32 *)((s8 *)(sourceWords) + (0x5C))), (*(s32 *)((s8 *)(sourceWords) + (0x64))));
            func_0021D25C(8, temp_s6_230 + var_s8_30 + sp9C, sourceWords, 0xFF,
                ((*(s32 *)((s8 *)(sourceWords) + (0x4C))) == 0x4A) ? 7 : 0xB);
            func_0021D25C(9, temp_s6_230 + var_s8_30 + sp9C + spA4, sourceWords, 0xFF, 0xC);
        }
        (*(s32 *)((s8 *)(sourceWords) + (0x84))) = var_s8_30;
        temp_v1_946 = spB4 - spAC;
        (*(s32 *)((s8 *)(sourceWords) + (0x88))) = temp_v1_946;
        temp_v0_951 = (sp9C + spA4) - temp_v1_946;
        (*(s32 *)((s8 *)(sourceWords) + (0x8C))) = temp_v0_951;
        if (temp_v0_951 < 0) {
            (*(s32 *)((s8 *)(sourceWords) + (0x8C))) = 0;
        }
        var_v0_63 = 1;
        (*(s32 *)((s8 *)(sourceWords) + (0x94))) = (s32) (temp_s6_230 + var_s8_30 + sp9C + spA4);
        var_a0_58 = temp_s6_230 + (*(s32 *)((s8 *)(sourceWords) + (0x84))) + (*(s32 *)((s8 *)(sourceWords) + (0x88)));
    }
    *outputWord = var_a0_58;
    return 1;
}
