typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern s16 D_8021AFB0;
extern u8 D_8021AFB2;
extern s8 D_8021AFB3;
extern u8 D_8021AFB4;
extern u8 D_80214E84;
void func_0000735C(s32 a, void *b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_00171F1C(void);
void func_00171EA0(s16 a, u8 b, u8 c, u8 d) {
    D_8021AFB0 = a;
    D_8021AFB2 = b;
    D_8021AFB3 = c;
    D_8021AFB4 = d;
    func_0000735C(0xC000, func_00171F1C, 0x66, 0x41, 0x70, 0x4B, 0xC8);
    D_80214E84 = 1;
}
void func_00171F1C(void);
