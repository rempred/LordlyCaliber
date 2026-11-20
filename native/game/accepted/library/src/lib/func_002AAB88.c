typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 *D_8022A974;
s16 func_002AAB88(s32 id, s32 mode) {
    s32 r = -1;
    s32 i;
    u8 *o;
    for (i = 0; i < 28; i++) {
        o = (*(u8 * *)((u8 *)(D_8022A974 + (i) * 4) + (0x18)));
        if (o == ((void *)0)) {
            if (mode != 1) {
                r = i;
            }
        } else if (o[0x147] == id) {
            if (mode == 1) {
                return i;
            }
            return -1;
        }
    }
    return r;
}
