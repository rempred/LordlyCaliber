typedef unsigned short u16;
typedef signed int s32;
extern u16 D_800C4B28;
s32 func_002864F8(s32 mask) {
    s32 r;
    if (mask) {
        r = (D_800C4B28 & mask) == 0;
    } else {
        r = D_800C4B28;
    }
    return r;
}
