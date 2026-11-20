typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00[2];
    u16 hi;
    u8 mid;
    u8 lo;
} TimeStamp;
TimeStamp *func_0028ab74(u8 i);
s16 func_00294FEC(s32 n) {
    s32 g = 0;
    u8 mid = 0;
    u16 hi = 0;
    s32 i;
    TimeStamp *t;
    for (i = 0; i < 121; i++) {
        t = func_0028ab74(i);
        if (t == ((void *)0)) {
            break;
        }
        if (hi != t->hi) {
            hi = t->hi;
            mid = t->mid;
            g++;
        } else if (mid != t->mid) {
            g++;
            mid = t->mid;
        }
        if (n == g - 1) {
            return i;
        }
    }
    return g;
}
