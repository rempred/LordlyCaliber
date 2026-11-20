typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
inline s32 func_000420d0(u16 year, u8 era) {
    s32 r;
    if (era) {
        year += 1748;
    }
    r = 0;
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
        r = 1;
    }
    return r;
}
