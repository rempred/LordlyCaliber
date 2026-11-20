typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
void func_00145C4C(u8 *src, u8 *img, u16 alt, u16 *dst, s32 n) {
    u16 *pal;
    if (img[0] < 2) {
        pal = (u16 *)(img + 8);
    } else if (alt == 0) {
        pal = (u16 *)(img + 8);
    } else {
        pal = (u16 *)(img + 0x208);
    }
    src += n / 2;
    for (; n > 0; n--) {
        *dst++ = pal[*src++];
    }
}
