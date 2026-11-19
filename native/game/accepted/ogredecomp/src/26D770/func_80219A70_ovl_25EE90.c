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
    u8 unk00[0x3C];
    s32 w3C;
    s32 w40;
    s32 w44;
    s32 w48;
    s32 w4C;
    s32 w50;
} Task26D;
extern s32 D_80220F68_ovl_25EE90;
Task26D *func_8020DF00_ovl_25EE90(void *fn);
void func_80219140_ovl_25EE90(void);
s32 func_8021A57C_ovl_25EE90(u16 msg, u8 *o);
void func_80219A9C_ovl_25EE90(void);
u8 *func_80217460_ovl_25EE90(s32 id);
void func_8020DC80_ovl_25EE90(u16 state);
s32 func_8021B500_ovl_25EE90(u16 msg, u8 *o);
s32 func_80219970_ovl_25EE90(u16 msg, u8 *o);
void func_80219A18_ovl_25EE90(s16 a, s32 b, s32 c);
void func_80219A70_ovl_25EE90(s32 a) {
    D_80220F68_ovl_25EE90 = a;
    func_8020DF00_ovl_25EE90(func_8021A57C_ovl_25EE90);
}
void func_8021A210_ovl_25EE90(s16 a, s32 b, s32 c);
s32 func_8021A57C_ovl_25EE90(u16 msg, u8 *o);
