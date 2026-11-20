typedef unsigned short u16;
typedef signed int s32;
extern u16 D_800E79BC;
s32 func_000b31c0(void) {
    u16 b = D_800E79BC;
    s32 r = -1;
    if (b & 8) {
        r = 0;
    } else if (b & 4) {
        r = 1;
    } else if (b & 2) {
        r = 2;
    } else if (b & 1) {
        r = 3;
    }
    return r;
}
