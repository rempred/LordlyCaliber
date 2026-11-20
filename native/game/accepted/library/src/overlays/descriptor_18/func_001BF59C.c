typedef unsigned char u8;
typedef signed int s32;
s32 D_8017B550(u8 c);
extern u8 D_8022A0A8;
extern u8 D_8022A0A4;
extern u8 D_8022A09D;
extern u8 D_8022A09C;
extern u8 D_8022A099;
extern u8 D_8022A098;
s32 func_001BF59C(u8 *c) {
    s32 n;
    s32 i;
    u8 buf[16];
    if (c[0] == 0) {
        return 0;
    }
    if (D_8017B550(c[0])) {
        n = 2;
        if (c[0] == D_8022A098 && c[1] == D_8022A099 && c[2] == D_8022A09C &&
            c[3] == D_8022A09D) {
            n = 4;
        }
        return n;
    }
    n = 1;
    if (c[0] == D_8022A0A4) {
        for (i = 1; c[i] != D_8022A0A8; i++) {
            n++;
        }
        n++;
    }
    return n;
}
