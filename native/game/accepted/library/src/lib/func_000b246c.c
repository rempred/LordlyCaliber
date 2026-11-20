typedef unsigned short u16;
typedef signed int s32;
extern u16 D_800E8700;
s32 func_000b246c(void) {
    u16 b = D_800E8700;
    s32 r = -1;
    if (b & 0x800) {
        r = 0;
    } else if (b & 0x400) {
        r = 1;
    } else if (b & 0x200) {
        r = 2;
    } else if (b & 0x100) {
        r = 3;
    }
    return r;
}
