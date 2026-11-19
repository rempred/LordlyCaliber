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
extern s32 D_801F3658;
extern s32 D_801F1074;
extern u8 *D_801F0CB0[];
extern u8 D_80214A60;
extern u8 D_80214A62;
extern u8 D_80214A63;
extern u8 D_801F365D;
extern s32 D_801F0DE0;
extern u8 D_801F0FDF;
extern u8 D_801F0FE0;
extern u8 D_801F0FE1;
void func_801B2D2C(u8 *u);
void func_801C8EAC(s32 a);
u8 func_801DADF0(u8 *u);
extern u16 *D_800C4BDC;
extern u16 *D_801F0CA0;
extern s32 D_8018F58C;
extern s32 D_801F0DE8;
extern u8 D_80210219;
extern u8 D_801F105F;
void func_801B8D44(f32 x, f32 y, f32 z, s32 a, s32 b);
void func_80216BF0_ovl_165FC0(void);
void func_8020CDA0(s32 a);
void func_8021E950_ovl_165FC0(void);
void func_8021EA50_ovl_165FC0(void) {
    s32 cur = D_801F3658;
    u8 *u = D_801F0CB0[cur];
    s32 sel;
    if ((u8)(D_80214A60 - 1) < 2) {
        if (D_80214A60 == 1) {
            sel = D_801F1074;
            func_801B2D2C(u);
            func_801C8EAC(1);
            *(u32 *)u &= ~2;
            D_801F3658 = cur;
            D_801F1074 = sel;
            D_801F365D |= 1;
            u[0x91] = D_80214A62;
            u[0x92] = D_80214A63;
        }
        D_801F0DE0 = 4;
        D_801F0FE1 = func_801DADF0(u);
        D_801F0FDF = 1;
        D_801F0FE0 = 2;
    }
}
