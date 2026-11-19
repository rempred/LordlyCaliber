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
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile s32 vs32;
extern u8 D_800AA050[][2];
void func_8008A910(u8 *jis, u8 *n64, u32 len) {
    u32 cnt;
    u32 cnt2;
    for (cnt = 0; cnt < len; cnt++) {
        if (*jis == 0) {
            break;
        }
        if ((*jis >= 'A') && (*jis <= 'Z')) {
            *n64 = *jis - 0x27;
        } else if (((u8)(*jis - 'a')) < 26) {
            *n64 = *jis - 0x47;
        } else if ((*jis >= '0') && (*jis <= '9')) {
            *n64 = *jis - 0x20;
        } else if ((*jis >= 0xB1) && (*jis <= 0xDC)) {
            if (*(jis + 1) == 0xDE) {
                if (*jis < 0xCA) {
                    *n64 = *jis - 0x3A;
                } else {
                    *n64 = *jis - 0x3F;
                }
                jis++;
            } else if (*(jis + 1) == 0xDF) {
                *n64 = *jis - 0x3A;
                jis++;
            } else {
                *n64 = *jis - 0x61;
            }
        } else {
            for (cnt2 = 0; D_800AA050[cnt2][1]; cnt2++) {
                if (D_800AA050[cnt2][0] == *jis) {
                    *n64 = D_800AA050[cnt2][1];
                    break;
                }
            }
            if (!D_800AA050[cnt2][1]) {
                *n64 = 0xF;
            }
        }
        n64++;
        jis++;
    }
}
void func_8008AA5C(u8 *n64, u8 *jis, u32 len);
