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
extern u32 D_800C4B30;
u32 osSetIntMask(u32 mask);
void func_8008A5A0(void);
void func_8008A5D0(void) {
    u32 mask;
    mask = osSetIntMask(1);
    D_800C4B30 = 0;
    osSetIntMask(mask);
}
