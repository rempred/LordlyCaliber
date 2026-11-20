typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long long u64;
typedef struct {
    u16 hours;
    u8 minutes;
    u8 seconds;
} Clock420;
u64 osGetTime(void);
void func_00042a44(Clock420 *c) {
    u64 s = osGetTime() / 46875000;
    c->seconds = s % 60;
    c->minutes = s / 60 % 60;
    s /= 3600;
    if (s < 100) {
        c->hours = s;
    } else {
        c->hours = 99;
        c->seconds = 59;
        c->minutes = 59;
    }
}
