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
void func_8008A910(u8 *jis, u8 *n64, u32 len);
void func_8008AA5C(u8 *n64, u8 *jis, u32 len) {
    u32 cnt;
    u32 cnt2;
    for (cnt = 0; cnt < len; cnt++) {
        if (*n64 == 0) {
            *jis = *n64;
        } else if ((*n64 >= 0x1A) && (*n64 <= 0x33)) {
            *jis = *n64 + 0x27;
        } else if ((*n64 >= 0x10) && (*n64 <= 0x19)) {
            *jis = *n64 + 0x20;
        } else if ((*n64 >= 0x50) && (*n64 <= 0x7B)) {
            *jis = *n64 + 0x61;
        } else if ((*n64 >= 0x7C) && (*n64 <= 0x8A)) {
            *jis = *n64 + 0x3A;
            *(jis + 1) = 0xDE;
            jis++;
        } else if ((*n64 >= 0x8B) && (*n64 <= 0x8F)) {
            *jis = *n64 + 0x3F;
            *(jis + 1) = 0xDE;
        } else if ((*n64 >= 0x90) && (*n64 <= 0x94)) {
            *jis = *n64 + 0x3A;
            *(jis + 1) = 0xDF;
        } else {
            for (cnt2 = 0; D_800AA050[cnt2][1]; cnt2++) {
                if (D_800AA050[cnt2][1] == *n64) {
                    *jis = D_800AA050[cnt2][0];
                    break;
                }
            }
            if (!D_800AA050[cnt2][1]) {
                *jis = 0xF;
            }
        }
        jis++;
        n64++;
    }
}
