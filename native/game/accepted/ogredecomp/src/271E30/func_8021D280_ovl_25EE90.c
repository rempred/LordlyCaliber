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
typedef struct {
    u8 unk00[0x30];
    s32 w30;
    s32 w34;
    s32 w38;
    s32 w3C;
} Task271;
extern s32 D_80220F68_ovl_25EE90;
Task271 *func_8020DF00_ovl_25EE90(void *fn);
s32 func_8021DACC_ovl_25EE90(u16 msg, u8 *o);
void func_8021D2AC_ovl_25EE90(void);
u8 *func_80217460_ovl_25EE90(s32 id);
void func_8020DC80_ovl_25EE90(u16 state);
s32 func_8021B500_ovl_25EE90(u16 msg, u8 *o);
void func_8021D280_ovl_25EE90(s32 a) {
    D_80220F68_ovl_25EE90 = a;
    func_8020DF00_ovl_25EE90(func_8021DACC_ovl_25EE90);
}
s32 func_8021DACC_ovl_25EE90(u16 msg, u8 *o);
void func_8021DB6C_ovl_25EE90(s16 a, s32 b, s32 c, s32 d);
