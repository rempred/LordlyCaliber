typedef unsigned char u8;
typedef unsigned short u16;
typedef struct {
    u16 hours;
    u8 minutes;
    u8 seconds;
} Clock420;
inline u8 func_00042428(Clock420 *c, u8 mode) {
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
