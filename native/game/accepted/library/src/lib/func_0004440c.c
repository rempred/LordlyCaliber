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
s32 func_00044130(s32, s32, M2C_UNK);
u8 func_000454e0(s32);
s32 func_00045934(s32, s32, s32, s32);
s32 func_00045a50(s32, s32, s32, s32);

u8 func_0004440c(u8 *arg0, s32 arg1, s32 arg2, s32 arg3, u8 arg4, u16 arg5, u16 arg6, u16 arg7, u16 arg8) {
    u8 temp_a0_204;
    s32 temp_a0_236;
    s32 temp_a0_56;
    s32 temp_a1_110;
    s32 temp_a1_137;
    s32 temp_a1_83;
    s32 temp_s2_181;
    s32 temp_s3_179;
    s32 temp_s4_177;
    s32 temp_s5_175;
    s32 temp_v1_106;
    s32 temp_v1_133;
    s32 temp_v1_235;
    s32 temp_v1_52;
    s32 temp_v1_79;
    s32 var_s1_184;
    s32 var_v1_195;
    s32 loaded_0;
    s32 loaded_1;
    s32 loaded_2;
    s32 loaded_3;
    s32 saved_arg1;
    s32 saved_arg2;
    u32 category;
    u32 final_category;
    int var_t0_13;
    int var_t1_14;
    s32 case_value;
    u8 local_result;

    local_result = 0;
    saved_arg1 = arg1;
    saved_arg2 = arg2;
    arg2 = arg4;
    arg4 = arg2;
    var_t0_13 = arg6;
    var_t0_13++;
    var_t0_13--;
    var_t1_14 = arg7;
    var_t1_14++;
    var_t1_14--;
    arg8++;
    arg8--;
    if (arg0 != 0) {
        *arg0 = arg3;
    }
    arg1 = (*(u8 *)((s8 *)((((arg3 & 0xFF) * 0x10) + 0x80190000)) + (-0x5580)));
    category = arg1;
    if ((category >= 2U) && (((category == 2) | (category == 5)) == 0)) {
        if (!(arg5 & 0xFFFF)) {
            temp_v1_52 = (saved_arg1 & 0xFF) * 0x48;
            temp_a0_56 = saved_arg2 & 0xFF;
            if ((*(u8 *)((s8 *)((temp_v1_52 + 0x80180000)) + (0x7C59))) == temp_a0_56) {
                do {
                    loaded_0 = (*(u16 *)((s8 *)((temp_v1_52 + 0x80180000)) + (0x7C42)));
                } while (0);
            } else {
                do {
                    loaded_0 = (*(u16 *)((s8 *)(((temp_a0_56 * 0x48) + 0x80180000)) + (0x7C42)));
                } while (0);
            }
            arg5 = loaded_0;
        }
        if ((var_t0_13 & 0xFFFF) == 0) {
            temp_v1_79 = (saved_arg1 & 0xFF) * 0x48;
            temp_a1_83 = saved_arg2 & 0xFF;
            if ((*(u8 *)((s8 *)((temp_v1_79 + 0x80180000)) + (0x7C59))) == temp_a1_83) {
                do {
                    loaded_1 = (*(u16 *)((s8 *)((temp_v1_79 + 0x80180000)) + (0x7C44)));
                } while (0);
            } else {
                do {
                    loaded_1 = (*(u16 *)((s8 *)(((temp_a1_83 * 0x48) + 0x80180000)) + (0x7C44)));
                } while (0);
            }
            var_t0_13 = loaded_1;
        }
        if ((var_t1_14 & 0xFFFF) == 0) {
            temp_v1_106 = (saved_arg1 & 0xFF) * 0x48;
            temp_a1_110 = saved_arg2 & 0xFF;
            if ((*(u8 *)((s8 *)((temp_v1_106 + 0x80180000)) + (0x7C59))) == temp_a1_110) {
                do {
                    loaded_2 = (*(u16 *)((s8 *)((temp_v1_106 + 0x80180000)) + (0x7C46)));
                } while (0);
            } else {
                do {
                    loaded_2 = (*(u16 *)((s8 *)(((temp_a1_110 * 0x48) + 0x80180000)) + (0x7C46)));
                } while (0);
            }
            var_t1_14 = loaded_2;
        }
        if ((arg8 & 0xFFFF) == 0) {
            temp_v1_133 = (saved_arg1 & 0xFF) * 0x48;
            temp_a1_137 = saved_arg2 & 0xFF;
            if ((*(u8 *)((s8 *)((temp_v1_133 + 0x80180000)) + (0x7C59))) == temp_a1_137) {
                do {
                    loaded_3 = (*(u16 *)((s8 *)((temp_v1_133 + 0x80180000)) + (0x7C48)));
                } while (0);
            } else {
                do {
                    loaded_3 = (*(u16 *)((s8 *)(((temp_a1_137 * 0x48) + 0x80180000)) + (0x7C48)));
                } while (0);
            }
            arg8 = loaded_3;
        }
        if ((arg5 & 0xFFFF) == 0xF3) {
            arg5 += arg4;
        }
        if ((var_t0_13 & 0xFFFF) == 0xF3) {
            var_t0_13 += arg4;
        }
        if ((var_t1_14 & 0xFFFF) == 0xF3) {
            var_t1_14 += arg4;
        }
        if ((arg8 & 0xFFFF) == 0xF3) {
            arg8 += arg4;
        }
        temp_s5_175 = arg5 & 0xFFFF;
        temp_s4_177 = var_t0_13 & 0xFFFF;
        temp_s3_179 = var_t1_14 & 0xFFFF;
        temp_s2_181 = arg8 & 0xFFFF;
        var_s1_184 = func_00045a50(temp_s5_175, temp_s4_177, temp_s3_179, temp_s2_181);
        if ((var_s1_184 & 0xFFFF) || (var_s1_184 = func_00045934(temp_s5_175, temp_s4_177, temp_s3_179, temp_s2_181), var_v1_195 = arg3 - 0x2D, ((var_s1_184 & 0xFFFF) != 0))) {
            local_result = func_000454e0(var_s1_184 & 0xFFFF);
            var_v1_195 = arg3 - 0x2D;
        }
        if ((((((u8) var_v1_195 < 2U) | ((temp_a0_204 = (u8) arg3), (temp_a0_204 == 0x2F))) != 0) || (temp_a0_204 == 0x30)) && (((var_s1_184 & 0xFFFF) == 0) || (((local_result == 0) | ((u8) local_result >= 8U)) != 0))) {
            arg3 = func_00044130(saved_arg1 & 0xFF, saved_arg2 & 0xFF, 0);
            if (arg0 != 0) {
                *arg0 = arg3;
            }
            goto compute_category;
        }
        temp_v1_235 = arg3 & 0xFF;
        if (local_result == 7) {
            temp_a0_236 = var_s1_184 & 0xFFFF;
            if (temp_a0_236 == 0xF9) {
                if (arg0 != 0) {
                    *arg0 = 0x63;
                }
                goto block_108;
            }
            if (temp_a0_236 == 0xFA) {
                if (arg0 != 0) {
                    *arg0 = 0x64;
                }
                goto block_108;
            }
            if (temp_a0_236 == 0xFB) {
                if (arg0 != 0) {
                    *arg0 = 0x65;
                }
                goto block_108;
            }
            if (temp_a0_236 == 0xFC) {
                if (arg0 != 0) {
                    *arg0 = 0x66;
                }
                goto block_108;
            }
            return 3U;
        } else {
            switch (temp_v1_235) {
            case 0x2D:
                case_value = 3;
                if (arg0 == 0) {
                    return case_value;
                }
                do {
                    case_value = (*(u8 *)((s8 *)((local_result + 0x80190000)) + (-0x4B94)));
                } while (0);
                *arg0 = case_value;
                return 3U;
            case 0x2F:
                case_value = 3;
                if (arg0 == 0) {
                    return case_value;
                }
                do {
                    case_value = (*(u8 *)((s8 *)((local_result + 0x80190000)) + (-0x4B84)));
                } while (0);
                *arg0 = case_value;
                return 3U;
            case 0x2E:
                case_value = 3;
                if (arg0 == 0) {
                    return case_value;
                }
                do {
                    case_value = (*(u8 *)((s8 *)((local_result + 0x80190000)) + (-0x4B8C)));
                } while (0);
                *arg0 = case_value;
                return 3U;
            case 0x30:
                case_value = 3;
                if (arg0 == 0) {
                    return case_value;
                }
                do {
                    case_value = (*(u8 *)((s8 *)((local_result + 0x80190000)) + (-0x4B7C)));
                } while (0);
                *arg0 = case_value;
                return 3U;
            case 0x33:
                if (arg0 != 0) {
                    *arg0 = 0x37;
                }
                return 3U;
            case 0x34:
                if (arg0 != 0) {
                    *arg0 = 0x57;
                }
                return 3U;
            case 0x35:
                if (arg0 != 0) {
                    *arg0 = 0x59;
                }
                return 3U;
            case 0x36:
                if (arg0 != 0) {
                    *arg0 = 0x5F;
                }
                return 3U;
            case 0x91:
                goto case_91;
            default:
                goto compute_category;
            }
block_108:
            return 3U;
case_91:
            do {
                case_value = 4;
                if (arg0 == 0) {
                    return case_value;
                }
                do {
                    case_value = (*(u8 *)((s8 *)((local_result + 0x80190000)) + (-0x4B74)));
                } while (0);
                *arg0 = case_value;
            } while (0);
            return 4U;
        }
    } else {
        return category;
    }

compute_category:
    final_category = arg3 & 0xFF;
block_114:
    final_category = (*(u8 *)((s8 *)(((final_category * 0x10) + 0x80190000)) + (-0x5580)));
    return final_category;
}
