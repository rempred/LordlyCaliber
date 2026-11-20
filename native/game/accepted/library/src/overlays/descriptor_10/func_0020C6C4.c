typedef unsigned char u8;
typedef signed int s32;
typedef unsigned int u32;
u8 func_002050AC(u32 a, s32 w, s32 i, u8 *c, u8 *d1, u8 *d2);
void func_0020C6C4(u8 *t, s32 *prev, s32 *last) {
    u8 c;
    u8 d1;
    u8 d2;
    s32 i = 0;
    u8 r;
    *last = 0;
    *prev = 0;
    for (;;) {
        r = func_002050AC((*(u32 *)((u8 *)(t) + (0))), (t[0x48] & 1) ? 0x58 : 0x26, i, &c, &d1, &d2);
        if (r == 0) {
            break;
        }
        i++;
        if (r == 1) {
            *prev = *last;
            *last = c;
        }
    }
}
