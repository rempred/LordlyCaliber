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
extern u8 D_80187020_ovl_040E80;
extern u8 D_80187021_ovl_040E80;
u8 func_8016CD30_ovl_040E80(void);
u8 func_8016CD3C_ovl_040E80(void);
void func_8016CD50_ovl_040E80(u8 a);
void func_8016CD5C_ovl_040E80(u8 a) {
    if (D_80187020_ovl_040E80 != a) {
        D_80187021_ovl_040E80 = a + 1;
    }
    D_80187020_ovl_040E80 = a;
}
