typedef unsigned char u8;
typedef signed int s32;
s32 func_000484c0(u8 c) {
    s32 r = 0;
    if ((u8)(c - 0x2D) < 2 || c == 0x2F || (u8)(c - 0x24) < 2 || c == 0x30) {
        r = 1;
    }
    return r;
}
