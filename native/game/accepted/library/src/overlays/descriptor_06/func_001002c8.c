typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_801B3E20;
s32 func_001002c8(void) {
    u8 *q = D_801B3E20;
    s32 r = 0;
    if ((*(s32 *)((u8 *)(q) + (0x298))) < 130) {
        (*(s32 *)((u8 *)(q) + (0x298)))++;
    } else {
        r = 1;
    }
    return r;
}
