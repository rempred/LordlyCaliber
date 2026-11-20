typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
u8 *func_001D03CC(u8 *t, u32 page);
u8 *func_001D03CC(u8 *t, u32 page) {
    u32 w;
    s32 h;
    u32 pix;
    u32 sub;
    u32 off;
    u32 i;
    u8 *e;
    u8 f;
    u8 g;
    if (t == ((void *)0)) {
        return ((void *)0);
    }
    f = t[3];
    w = *(u16 *)(t + 4);
    h = *(u16 *)(t + 6);
    if (f & 2) {
        pix = ((w + 3) >> 2 << 4) * h;
        sub = 0;
    } else if (f & 4) {
        pix = ((w + 7) >> 3 << 3) * h;
        if (f & 1) {
            sub = ((w + 15) >> 4 << 3) * h;
        } else {
            sub = pix;
        }
    } else {
        pix = ((w + 3) >> 2 << 3) * h;
        if (f & 1) {
            sub = ((w + 15) >> 4 << 3) * h;
        } else {
            sub = ((w + 7) >> 3 << 3) * h;
        }
    }
    off = 8;
    e = t + 8;
    for (i = 0; i < t[2]; i++) {
        if (i == page) {
            return e;
        }
        g = e[3];
        off += 8;
        if (g & 1) {
            off += pix;
        }
        if (g & 2) {
            off += sub;
        }
        if (g & 4) {
            off += 0x200;
        }
        e = t + off;
    }
    return ((void *)0);
}
