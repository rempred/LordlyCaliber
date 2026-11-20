typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef struct {
    u8 unk00[2];
    u16 hi;
    u8 mid;
    u8 lo;
} TimeStamp;
s32 func_0028a9dc(TimeStamp *a, TimeStamp *b) {
    s32 va = a->lo + a->mid * 30 + a->hi * 600;
    s32 vb = b->lo + b->mid * 30 + b->hi * 600;
    return vb < va;
}
