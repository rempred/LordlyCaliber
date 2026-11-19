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
extern void *D_800AA090;
u32 osSetIntMask(u32 mask);
void nuGfxTaskAllEndWait(void);
void func_80089990(void *func) {
    u32 mask;
    nuGfxTaskAllEndWait();
    mask = osSetIntMask(1);
    D_800AA090 = func;
    osSetIntMask(mask);
}
