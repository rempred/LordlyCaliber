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
extern u8 *D_80196AF8;
extern u8 D_801EF474_ovl_087200[];
extern s8 D_801EF480_ovl_087200[];
u8 func_801D6508_ovl_087200(void);
extern u8 D_801EF487_ovl_087200[];
u8 func_801D5A2C_ovl_087200(u8 m);
s32 func_801D5D10_ovl_087200(u8 *out) {
    u8 r;
    u8 none;
    *out = 0;
    r = func_801D6508_ovl_087200();
    none = r == 0;
    if (!none) {
        *out = D_801EF487_ovl_087200[r];
    }
    return none;
}
