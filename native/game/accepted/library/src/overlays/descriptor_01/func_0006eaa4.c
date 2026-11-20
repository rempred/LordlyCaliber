typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
u8 *func_0006e8d4(u8 *desc, u32 index);
u8 *func_0006e8d4(u8 *t, u32 page);
u8 *func_0006eaa4(u8 *desc, u32 index) {
    u32 w = *(u16 *)(desc + 4);
    u32 h = *(u16 *)(desc + 6);
    u8 *tex = func_0006e8d4(desc, index);
    u32 off;
    if (tex == ((void *)0)) {
        return ((void *)0);
    }
    off = 8;
    if (tex[3] & 1) {
        if (desc[3] & 2) {
            off = h * (((w + 3) >> 2) << 4) + 8;
        } else if (desc[3] & 4) {
            off = h * (((w + 7) >> 3) << 3) + 8;
        } else {
            off = h * (((w + 3) >> 2) << 3) + 8;
        }
    }
    if (tex[3] & 2) {
        if (desc[3] & 1) {
            off += h * (((w + 15) >> 4) << 3);
        } else {
            off += h * (((w + 7) >> 3) << 3);
        }
    }
    if (tex[3] & 4) {
        return tex + off;
    }
    return ((void *)0);
}
