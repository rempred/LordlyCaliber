typedef unsigned char u8;
typedef signed int s32;
s32 func_00051450(u8 c);
s32 func_00051450(u8 c) {
    if (c < 0x81 || (u8)(c + 0x60) < 0x40 || c >= 0xFD) {
        return 0;
    }
    return 1;
}
