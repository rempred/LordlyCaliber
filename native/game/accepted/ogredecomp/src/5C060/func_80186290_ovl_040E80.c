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
extern u8 gFlags[];
extern u8 D_80197AE7_ovl_040E80[];
extern s32 D_80196F58_ovl_040E80;
extern u8 D_801951A1_ovl_040E80;
void func_80186160_ovl_040E80(void);
void func_80186178_ovl_040E80(void);
void FlagSet(s32 flag, s32 value);
s32 FlagGet(s32 flag);
s32 func_80186240_ovl_040E80(s32 flag);
void func_80186284_ovl_040E80(s32 value);
void func_80186290_ovl_040E80(void) {
    D_801951A1_ovl_040E80 |= 1;
}
void func_801862A8_ovl_040E80(void);
