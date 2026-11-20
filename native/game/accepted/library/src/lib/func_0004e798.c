typedef unsigned short u16;
typedef signed int s32;
extern u16 D_8018F1A0;
void func_0004ec3c(void);
void func_00003564(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
void D_8022681C(s32 a, s32 b);
void func_0004e798(void) {
    u16 *p = &D_8018F1A0;
    *p = 0;
    func_0004ec3c();
    func_00003564(0, 8, 8, 0x80, 8, 0x80, 0x2800);
    D_8022681C(-5, *p & 0xFFF);
}
