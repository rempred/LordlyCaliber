typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
extern s8 (*D_80213054[])(u16 v);
u16 func_00152E8C(u8 side, u8 idx, u8 stat);
s16 func_00152DEC(u8 side, u8 idx, u8 k) {
    s16 sum = 0;
    s32 i;
    for (i = 1; i < 5; i++) {
        sum += D_80213054[k](func_00152E8C(side, idx, i));
    }
    return sum;
}
u16 func_00152E8C(u8 side, u8 idx, u8 stat);
