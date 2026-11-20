typedef unsigned char u8;
typedef signed int s32;
void func_00151F40(void *a, s32 b, s32 c);
void func_00153A98(u8 *p, s32 b) {
    func_00151F40(p, b, 2);
    func_00151F40(p + 0x2A, b, 7);
}
