typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
void func_00151F40(void *a, s32 b, s32 c);
void D_802001A8(void *a, s32 b, u16 c, s32 d);
void func_00153A14(u8 *p, s32 b, u16 c, u16 d) {
    func_00151F40(p, b, 2);
    D_802001A8(p + 0x14, b, c, -3);
    func_00151F40(p + 0x2A, b, 7);
    D_802001A8(p + 0x32, b, d, -3);
}
