typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
s32 D_801CA1C4(s32 lane, s32 row);
s16 func_0024C3A0(u8 c) {
    s32 v = c - 20;
    return D_801CA1C4(v % 3, v / 3);
}
