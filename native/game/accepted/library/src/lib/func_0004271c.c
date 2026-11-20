typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u16 hours;
    u8 minutes;
    u8 seconds;
} Clock420;
typedef struct {
    u16 year;
    u8 month;
    u8 day;
} Date420;
extern Clock420 D_80196A2C;
extern u8 D_80196A30[];
extern u8 D_80196A34;
extern u8 D_80196A35;
s32 func_000424bc(Date420 *d);
extern inline u8 func_00042428(Clock420 *c, u8 mode) {
    if (++c->seconds == 60) {
        c->minutes++;
        c->seconds = 0;
        if (c->minutes == 60) {
            c->hours++;
            c->minutes = 0;
            if (mode == 1) {
                if (c->hours == 24) {
                    c->hours = 0;
                    return 1;
                }
            } else if (c->hours == 0xFFFE) {
                c->hours--;
            }
        }
    }
    return 0;
}
s32 func_000424bc(Date420 *d);
void func_0004271c(void) {
    u8 *t = &D_80196A35;
    if (++*t >= D_80196A34) {
        *t = 0;
        if (func_00042428(&D_80196A2C, 1) == 1) {
            func_000424bc((Date420 *)D_80196A30);
        }
    }
}
