typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 *D_8022A978;
s32 func_002ADA20(void) {
    u8 *p = D_8022A978;
    s32 r = 0;
    if (p != ((void *)0)) {
        r = (*(s16 *)((u8 *)(p) + (0x8))) != (*(s16 *)((u8 *)(p) + (0xA)));
    }
    return r;
}
