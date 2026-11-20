typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_801B3E20;
s32 func_001002f8(void) {
    u8 *p = D_801B3E20;
    s32 r = 0;
    if ((*(s32 *)((u8 *)(p) + (0x2E4))) < 120) {
        (*(s32 *)((u8 *)(p) + (0x2E4))) += 2;
    } else {
        r = 1;
    }
    return r;
}
