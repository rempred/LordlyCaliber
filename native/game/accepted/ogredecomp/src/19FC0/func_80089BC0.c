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
extern u32 osTvType;
extern u32 D_800C4BD8;
void osViSetYScale(f32 scale);
void osViBlack(u8 active);
void func_80089BE4(u8 active);
void func_80089BC0(void) {
    D_800C4BD8 = 0;
    func_80089BE4(1);
}
void func_80089BE4(u8 active);
