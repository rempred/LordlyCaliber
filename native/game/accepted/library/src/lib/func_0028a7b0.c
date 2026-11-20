typedef unsigned char u8;
typedef signed int s32;
extern u8 D_8023E5FF[];
void func_0028a7b0(s32 unused, s32 n, u8 set) {
    u8 i;
    i = n - 1;
    if (set == 1) {
        D_8023E5FF[i / 8] |= 1 << (i % 8);
    } else {
        D_8023E5FF[i / 8] &= ~(1 << (i % 8));
    }
}
