typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00[2];
    u16 hi;
    u8 mid;
    u8 lo;
} TimeStamp;
TimeStamp *func_0028ab74(u8 i);
s32 func_00295574(s32 n) {
    s32 k = 0;
    u8 mid = 0;
    u16 hi = 0;
    s32 i;
    TimeStamp *t;
    for (i = 0; i < 121; i++) {
        t = func_0028ab74(i);
        if (t == ((void *)0)) {
            return 0;
        }
        if (hi != t->hi) {
            mid = t->mid;
            hi = t->hi;
            if (n == k) {
                return 1;
            }
            k++;
        } else if (mid != t->mid) {
            if (n == k) {
                return 1;
            }
            k++;
            mid = t->mid;
        }
        if (n == k) {
            return 0;
        }
        k++;
    }
    return 0;
}
