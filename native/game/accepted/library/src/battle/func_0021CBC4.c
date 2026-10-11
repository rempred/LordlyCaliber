typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;

extern void func_00093540(void *, ...);
extern s32 func_001CA29C(s32);
extern void func_001CD01C(void *, void *, s32);
extern u32 func_801D96A0(u32);
extern u8 * volatile D_801CE8C0;
extern u8 D_801E6AE0[];
extern u8 D_801E6B08[];

void func_0021CBC4(
    s32 arg0,
    s32 arg1,
    s32 arg2,
    s32 arg3,
    s32 *arg4,
    s32 *arg5,
    s32 *arg6,
    s32 arg7,
    s32 arg8)
{
    volatile u8 stack_pad[8];
    s32 *var_a0_178;
    s32 *var_a0_194;
    s32 *var_a0_218;
    s32 *var_a0_38;
    s32 temp_s0_149;
    s32 temp_s0_154;
    s32 temp_s0_171;
    s32 temp_s0_262;
    s32 temp_s0_264;
    s32 temp_s0_266;
    s32 temp_s0_268;
    s32 temp_s0_272;
    s32 temp_s0_293;
    s32 temp_s0_295;
    s32 temp_s0_297;
    s32 temp_s0_299;
    s32 temp_s0_304;
    s32 temp_s0_310;
    s32 temp_s0_323;
    s32 temp_s0_325;
    s32 temp_s0_327;
    s32 temp_s0_332;
    s32 temp_s0_345;
    s32 temp_s0_347;
    s32 temp_s0_349;
    s32 temp_s0_363;
    s32 temp_s0_365;
    s32 temp_s0_377;
    s32 temp_v0_226;
    s32 var_a1_201;
    s32 var_a1_221;
    s32 var_a1_48;
    s32 var_a3_246;
    s32 var_s0_181;
    s32 var_s0_195;
    s32 var_s3_34;
    u8 var_s6_96;
    s32 pad_kind;
    s32 pad_marker;
    s32 var_v0_184;
    s32 var_v0_204;
    u8 *temp_a0_376;
    u8 *var_v0_251;
    u8 *var_v1_183;
    u8 *var_v1_203;
    u8 *var_v1_222;
    u8 *var_v1_240;
    s32 var_s4_13;
    s32 var_v0_count;
    s32 var_v0_242;
    u32 var_s0_73;
    u8 *temp_v1_378;
    u8 temp_v1_76;
    u32 var_s3_29;
    u8 *temp_a1_137;
    u8 *temp_s1_23;
    u8 *temp_s2_118;
    u8 *temp_v1_125;
    u8 *temp_v1_409;
    u8 *var_a1_132;
    u8 *var_v0_74;

    var_s4_13 = arg1;
    temp_s1_23 = D_801CE8C0 + 0x10;
    if (arg0 == 0x16) {
        goto type_16_size;
    }
    var_s3_29 = arg3;
    if (arg0 == 0x38) {
        goto size_done;
    }
    goto regular_size;

type_16_size:
    var_s3_29 = 5;
    var_a0_38 = arg4 + 1;
    if (*arg4 != -1) {
        do {
            var_s3_29 += 1;
        } while (*var_a0_38++ != -1);
    }
    var_s3_29 += 1;
    var_a1_48 = 0;
    var_a0_38 = arg5 + 1;
    if (*arg5 != -1) {
        do {
            var_a1_48 += 1;
        } while (*var_a0_38++ != -1);
    }
    var_a1_48 += 1;
    var_s3_29 += var_a1_48 * 2;
    goto size_done;

regular_size:
    var_s3_29 = *(u8 *)(arg0 + 0x801E5C70);

size_done:
    var_s0_73 = 0;
    if (*(u32 *)(D_801CE8C0 + 0x814) != 0) {
loop_13:
        var_v0_74 = (u8 *)((s32)var_s0_73 - -(u32)temp_s1_23);
        temp_v1_76 = *(u8 *)(var_v0_74 + 2);
        if (var_s4_13 < (s32)temp_v1_76) {
            goto check_cursor;
        }
        var_s4_13 -= temp_v1_76;
        var_s0_73 = func_801D96A0(var_s0_73);
        if (var_s0_73 < *(u32 *)(D_801CE8C0 + 0x814)) {
            goto loop_13;
        }
check_cursor:
        if (var_s0_73 >= *(u32 *)(D_801CE8C0 + 0x814)) {
            var_s0_73 = *(volatile u32 *)(D_801CE8C0 + 0x814);
        }
    } else {
        var_s0_73 = *(volatile u32 *)(D_801CE8C0 + 0x814);
    }
    var_s6_96 = var_s4_13 < 0xFF;
    if (var_s6_96 == 0) {
        var_s3_29 += (var_s4_13 / 255) * 3;
    }
    temp_s2_118 = temp_s1_23 + var_s0_73;
    func_001CD01C(
        temp_s2_118 + var_s3_29,
        temp_s2_118,
        *(u32 *)(D_801CE8C0 + 0x814) - var_s0_73);
    temp_v1_125 = D_801CE8C0;
    *(u32 *)(temp_v1_125 + 0x814) += var_s3_29;
    if (var_s6_96 == 0) {
        pad_kind = 0x1E;
        pad_marker = 0xFF;
        var_a1_132 = temp_s2_118;
        do {
            *(u8 *)var_a1_132 = pad_kind;
            var_a1_132 += 1;
            var_s0_73 += 3;
            var_s4_13 -= 0xFF;
            *(u8 *)var_a1_132 = *(u8 *)(D_801CE8C0 + 0x818);
            var_a1_132 += 1;
            *(u8 *)var_a1_132 = pad_marker;
            var_a1_132 += 1;
        } while (var_s4_13 >= 0xFF);
    }
    temp_s1_23[var_s0_73++] = arg0;
    temp_s1_23[var_s0_73++] = *(u8 *)(D_801CE8C0 + 0x818);
    temp_s1_23[var_s0_73++] = var_s4_13;
    switch (arg0) {
    case 0x16:
        temp_s1_23[var_s0_73++] = var_s3_29 - 4;
        temp_s1_23[var_s0_73++] = arg3;
        var_a0_178 = arg4;
        if (*arg4 != -1) {
            do {
                temp_s1_23[var_s0_73++] = *var_a0_178;
                var_a0_178 += 1;
            } while (*var_a0_178 != -1);
        }
        temp_s1_23[var_s0_73++] = 0xFF;
        var_a0_194 = arg5;
        var_a1_201 = 0;
        if (*var_a0_194 != -1) {
            do {
                temp_v0_226 = *var_a0_194;
                var_a0_194 += 1;
                temp_s1_23[var_s0_73++] = temp_v0_226;
                var_a1_201 += 1;
            } while (*var_a0_194 != -1);
        }
        temp_s1_23[var_s0_73++] = 0xFF;
        var_a0_218 = arg6;
        if (var_a1_201-- != 0) {
            var_v1_222 = (u8 *)((s32)var_s0_73 - -(u32)temp_s1_23);
            do {
                if (var_a0_218 != 0) {
                    temp_v0_226 = *var_a0_218;
                    var_a0_218 += 1;
                    var_s0_73 += 1;
                    *var_v1_222 = temp_v0_226;
                    var_v1_222 += 1;
                } else {
                    *var_v1_222 = 0;
                    var_v1_222 += 1;
                    var_s0_73 += 1;
                }
            } while (var_a1_201-- != 0);
        }
        var_v1_240 = temp_s1_23 + var_s0_73;
        var_v0_242 = 0xFF;
        goto block_51;
    case 0x38:
        temp_s1_23[var_s0_73++] = arg3;
        var_a3_246 = arg3 - 5;
        if (var_a3_246 != -1) {
            do {
                temp_s1_23[var_s0_73++] = 0;
                var_a3_246 -= 1;
            } while (var_a3_246 != -1);
        }
        goto block_54;
    case 0x17:
        temp_s1_23[var_s0_73++] = arg3;
        temp_s1_23[var_s0_73++] = (u8)arg4;
        temp_s1_23[var_s0_73++] = (u8)arg5;
        temp_s1_23[var_s0_73++] = 0xFF;
        temp_s1_23[var_s0_73++] = (u8)arg6;
        temp_s1_23[var_s0_73++] = 0xFF;
        goto block_54;
    case 0x20:
        temp_s1_23[var_s0_73++] = arg3;
        goto block_54;
    case 0x1F:
        temp_s1_23[var_s0_73++] = func_001CA29C(arg2);
        temp_s1_23[var_s0_73++] = arg3;
        temp_s1_23[var_s0_73++] = (u8)arg4;
        temp_s1_23[var_s0_73++] = (u8)arg5;
        temp_s1_23[var_s0_73++] = (u8)arg6;
        temp_s1_23[var_s0_73++] = arg7;
        temp_s1_23[var_s0_73++] = arg8;
        goto block_54;
    case 0xB:
    case 0xC:
    case 0x10:
    case 0x32:
    case 0x3B:
        temp_s1_23[var_s0_73++] = func_001CA29C(arg2);
        temp_s1_23[var_s0_73++] = arg3;
        temp_s1_23[var_s0_73++] = (u8)arg4;
        temp_s1_23[var_s0_73++] = (u8)arg5;
        temp_s1_23[var_s0_73++] = (u8)arg6;
        goto block_54;
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x6:
    case 0x7:
    case 0xA:
    case 0x18:
    case 0x1A:
    case 0x21:
    case 0x36:
        temp_s1_23[var_s0_73++] = func_001CA29C(arg2);
        temp_s1_23[var_s0_73++] = arg3;
        temp_s1_23[var_s0_73++] = (u8)arg4;
        temp_s1_23[var_s0_73++] = (u8)arg5;
        goto block_54;
    case 0x5:
    case 0x8:
    case 0x9:
    case 0x11:
    case 0x1D:
    case 0x34:
    case 0x39:
        temp_s1_23[var_s0_73++] = func_001CA29C(arg2);
        temp_s1_23[var_s0_73++] = arg3;
        temp_s1_23[var_s0_73++] = (u8)arg4;
        goto block_54;
    case 0x1:
    case 0xD:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x19:
    case 0x1B:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x30:
    case 0x31:
    case 0x35:
    case 0x3A:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F:
        temp_s1_23[var_s0_73++] = func_001CA29C(arg2);
        temp_s1_23[var_s0_73++] = arg3;
        goto block_54;
    case 0x1C:
    case 0x22:
    case 0x2C:
    case 0x33:
    case 0x37:
        var_v0_242 = func_001CA29C(arg2);
        var_v1_240 = temp_s1_23 + var_s0_73;
        goto block_51;
    case 0x1E:
    case 0x40:
    case 0x41:
        goto block_54;
block_51:
        *var_v1_240 = var_v0_242;
        var_s0_73 += 1;
        goto block_54;
    default:
        func_00093540(D_801E6AE0, arg0);
loop_53:
        goto loop_53;
    }

block_54:
    if (var_s0_73 < *(u32 *)(D_801CE8C0 + 0x814)) {
        var_v0_74 = (u8 *)(var_s0_73 + (u32)temp_s1_23);
        if (*(volatile u8 *)(var_v0_74 + 2) < var_s4_13) {
            func_00093540(D_801E6B08);
loop_57:
            goto loop_57;
        }
        temp_v1_409 = (u8 *)((s32)var_s0_73 - -(u32)temp_s1_23);
        temp_v1_409[2] -= var_s4_13;
    }
}
