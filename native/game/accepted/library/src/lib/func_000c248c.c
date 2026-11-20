typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80196AF8;
extern u8 D_801EF474[];
extern s8 D_801EF480[];
u8 func_000c248c(u8 m) {
    u8 *st = D_80196AF8;
    s32 n = 0;
    s32 k;
    u8 *q;
    s32 pos;
    u8 mask;
    u8 c;
    q = st + ((*(u16 *)((u8 *)(st) + (0x18C))) * 0x36 + 0x117C);
    pos = (*(u16 *)((u8 *)(st) + (0x19C)));
    mask = D_801EF474[pos];
    for (k = 0; k < 8; k++) {
        if ((mask >> k) & 1) {
            c = (q + (pos + D_801EF480[k]))[4];
            if (c != 0xFF && c != m) {
                n++;
                break;
            }
        }
    }
    return n;
}
