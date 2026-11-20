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
s16 func_002953E0(s32 line) {
    s32 k = 0;
    u8 lastB = 0;
    u16 lastH = 0;
    s32 i;
    u8 *e;
    for (i = 0; i < 121; i++) {
        e = (u8 *)func_0028ab74(i);
        if (e == ((void *)0)) {
            break;
        }
        if (lastH != *(u16 *)(e + 2)) {
            lastB = e[4];
            if (line == k) {
                return i;
            }
            lastH = *(u16 *)(e + 2);
            k++;
        } else if (lastB != e[4]) {
            if (line == k) {
                return i;
            }
            k++;
            lastB = e[4];
        }
        if (line == k) {
            return i;
        }
        k++;
    }
    return k;
}
