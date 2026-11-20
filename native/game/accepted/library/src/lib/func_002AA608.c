typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
s32 func_002AA608(u8 *p) {
    s32 c = (*(s32 *)((u8 *)(p) + (0xE8)));
    s32 r;
    if ((u32)(c - 0x51) < 3) {
        r = 1;
    } else {
        r = c == 0xA3;
    }
    return r;
}
