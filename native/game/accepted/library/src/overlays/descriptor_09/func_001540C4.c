typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
void func_00151F40(void *a, s32 b, s32 c);
void func_001540C4(u8 *p, s32 b, u8 pct) {
    u16 v = pct / 14.285714f;
    if (v > 6) {
        v = 6;
    }
    switch (v) {
        case 0:
        case 1:
            func_00151F40(p - (v * 2 - 12), b + 8, 10);
            break;
        case 5:
        case 6:
            func_00151F40(p - (v * 2 - 12), b + 8, 12);
            break;
        default:
            func_00151F40(p - (v * 2 - 12), b + 8, 11);
            break;
    }
}
