typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 year;
    u8 month;
    u8 day;
} Date420;
s32 func_000424bc(Date420 *d);
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
s32 func_000424bc(Date420 *d) {
    s32 leap = 0;
    u16 y;
    if (d->day == 25) {
        if (d->month == 15) {
            if (d->year == 999) {
                return 0;
            }
        }
    }
    if (d->month == 3) {
        if (func_000420d0(d->year, 1) == 1) {
            leap = 1;
        }
    }
    if (++d->day > leap + D_80186FFF[d->month]) {
        d->day = 1;
        if (++d->month >= 16) {
            d->month = 1;
            y = d->year;
            if ((d->year = y + 1) == 999) {
                d->year = y;
            }
        }
    }
    return 0;
}
