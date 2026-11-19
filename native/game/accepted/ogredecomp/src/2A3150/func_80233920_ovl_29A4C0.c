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
u8 *func_80232E44_ovl_29A4C0(s32 a, s32 b);
u8 *func_80233920_ovl_29A4C0(s32 a, s32 i, s32 b) {
    u8 *p = func_80232E44_ovl_29A4C0(a, b);
    if (p != ((void *)0)) {
        return (u8 *)(((s32 *)p)[i] + (s32)p);
    }
    return ((void *)0);
}
