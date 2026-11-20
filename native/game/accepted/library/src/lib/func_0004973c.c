typedef unsigned char u8;
typedef signed int s32;
extern u8 D_801AD5A0[];
void func_00003564(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
s32 func_0000735C(s32 a, void *b, s32 c, s32 d, s32 e, s32 f, s32 g);
void func_0004973c(void) {
    func_00003564(0, 10, 10, 0x100, 10, 0x200, 0x2000);
    func_0000735C(0xC000, D_801AD5A0, 0, 0, 0x13F, 0xEF, 0xF000);
}
