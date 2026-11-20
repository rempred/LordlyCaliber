typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
void func_00145C14(u8 *src, u8 *dst, s32 n);
void func_00145C4C(u8 *src, u8 *img, u16 alt, u16 *dst, s32 n);
void func_0014621C(u8 *a, u8 *b, u8 *t) {
    s32 size;
    if (t[0xC] != t[0x12]) {
        for (;;) {
        }
    }
    switch (t[0xC]) {
        case 0:
            size = 0x400;
            break;
        case 1:
            size = 0x780;
            break;
        default:
            for (;;) {
            }
    }
    func_00145C4C(a, b, t[0xB], (*(u16 * *)((u8 *)(t) + (0))), size);
    func_00145C14(a, (*(u8 * *)((u8 *)(t) + (4))), size);
    (*(u16 *)((u8 *)(t) + (0xE))) = (*(u16 *)((u8 *)(t) + (8)));
    t[0x10] = t[0xA];
    t[0x11] = t[0xB];
    t[0x12] = t[0xC];
}
