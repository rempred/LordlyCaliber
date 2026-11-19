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
extern void *D_8021B588_ovl_188B60;
void *func_8017BFB0(s32 a, s32 b, void *c, s32 d, s32 e, s32 f, s32 g, s32 h, s32 i, s32 j, s32 k);
void func_80219584_ovl_188B60(void);
extern s32 D_8021B5A4_ovl_188B60;
extern s32 D_8021B5B0_ovl_188B60;
extern void *D_8021B58C_ovl_188B60;
void func_8021948C_ovl_188B60(void) {
    void **p = &D_8021B588_ovl_188B60;
    if (*p == ((void *)0)) {
        *p = func_8017BFB0(0, 0, func_80219584_ovl_188B60, 0x18, 0x18, 0x1C, 0x1C, 0x18, 0x18, 0x91, 0x4A);
    }
}
void func_80219500_ovl_188B60(void);
