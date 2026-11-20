typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_80196AF8;
void func_000b745c(void) {
    u8 *b = D_80196AF8 + 0x1160;
    s32 i;
    u8 s;
    for (i = 0; i < 5; i++) {
        s = (b + i)[2];
        if (s != 0xFF) {
            u8 *q = D_80196AF8 + (s * 0x36 + 0x117C);
            q[1] &= 0xFD;
        }
    }
}
