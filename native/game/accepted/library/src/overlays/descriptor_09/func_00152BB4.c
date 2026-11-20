typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
extern s8 (*D_80213054[])(u16 v);
u16 func_00152CD8(u8 side, u8 idx, u8 slot);
void func_00152C6C(u8 enemy, u8 idx, u8 *cls, u8 *base);
s16 func_00152BB4(s32 side, s32 idx, s32 k) {
    u8 cls;
    u8 base;
    s16 sum = 0;
    s32 i;
    func_00152C6C(side, idx, &cls, &base);
    for (i = 1; i < 5; i++) {
        sum += (s8)D_80213054[(u8)k](func_00152CD8(side, idx, i));
    }
    return sum;
}
u16 func_00152CD8(u8 side, u8 idx, u8 slot);
