typedef unsigned char u8;
typedef signed int s32;
u8 func_00213B24(u8 *p) {
    if (*(s32 *)(p + 0x48) == 0x101) {
        return p[0x33];
    }
    return p[0xA4];
}
