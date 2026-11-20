typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 year;
    u8 month;
    u8 day;
} Date420;
extern u8 D_80186FFF[];
extern inline s32 func_000420d0(u16 year, u8 era) {
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
s32 func_0004260c(Date420 *d) {
    s32 leap = 0;
    if (d->month == 4) {
        if (func_000420d0(d->year, 1) == 1) {
            leap = 1;
        }
    }
    if (--d->day == 0) {
        if (--d->month == 0) {
            d->month = 15;
            d->year--;
        }
        d->day = D_80186FFF[d->month] + leap;
    }
    return 0;
}
