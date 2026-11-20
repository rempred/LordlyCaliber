typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_801CE8BC;
s32 D_801C8C90(u8 *u);
s32 func_002A8DBC(u8 *o) {
    u8 *u;
    s32 r;
    if (o == ((void *)0) || o[0x147] == 0xFF) {
        return 0;
    }
    u = D_801CE8BC + 0x1C4 + o[0x147] * 0xF8;
    if ((*(s32 *)((u8 *)(u) + (0x48))) == 0) {
        return 0;
    }
    r = D_801C8C90(u);
    if (r == 1) {
        if (o[0x149] == r) {
            return 2;
        }
        return 1;
    }
    return 0;
}
