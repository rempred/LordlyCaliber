typedef unsigned short u16;
typedef signed int s32;
u16 func_0005084c(u16 c) {
    u16 hi = (c & 0x7F7F) >> 8;
    u16 lo = c & 0x7F;
    s32 h;
    if (hi & 1) {
        lo += 0x1F;
    } else {
        lo += 0x7D;
    }
    if (lo >= 0x7F) {
        lo++;
    }
    h = (hi - 0x21) >> 1;
    hi = h + 0x81;
    if (hi >= 0xA0) {
        hi = h + 0xC1;
    }
    return lo | (hi << 8);
}
