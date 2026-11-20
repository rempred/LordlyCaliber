typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_80196AF8;
u8 func_000b8600(void) {
    u8 *st = D_80196AF8;
    s32 i;
    for (i = 0; i < 30; i++) {
        if (st[0x117F + i * 0x36] == 0xFF) {
            break;
        }
    }
    return i;
}
