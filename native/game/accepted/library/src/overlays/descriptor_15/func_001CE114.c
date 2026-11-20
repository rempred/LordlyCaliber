typedef signed int s32;
typedef unsigned int u32;
u32 func_001CE114(s32 siz, u32 w, s32 h) {
    u32 r;
    if (siz == 3) {
        r = ((w + 3) >> 2) << 4;
    } else if (siz == 2) {
        r = ((w + 3) >> 2) << 3;
    } else if (siz == 1) {
        r = ((w + 7) >> 3) << 3;
    } else {
        r = ((w + 15) >> 4) << 3;
    }
    return r * h;
}
