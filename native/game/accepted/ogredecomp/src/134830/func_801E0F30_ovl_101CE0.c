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
extern s32 D_801ED4A4_ovl_101CE0[];
s32 func_801E0F30_ovl_101CE0(s32 a) {
    s32 *p = D_801ED4A4_ovl_101CE0;
    s32 i = 0;
    while (*p != a) {
        p += 3;
        i += 3;
    }
    return i;
}
