typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
u8 *func_001B7630(u8 *t, u32 page);
u8 *func_001B7630(u8 *t, u32 page);
u8 *func_001B7754(u8 *t, u32 page) {
    u8 *e = func_001B7630(t, page);
    u16 w;
    s32 h;
    s32 stride;
    if (e == ((void *)0)) {
        return ((void *)0);
    }
    w = *(u16 *)(t + 4);
    h = *(u16 *)(t + 6);
    if (e[3] & 2) {
        if (!(e[3] & 1)) {
            return e + 8;
        }
        if (t[3] & 2) {
            stride = ((u32)(w + 3) >> 2) << 4;
        } else if (t[3] & 4) {
            stride = ((u32)(w + 7) >> 3) << 3;
        } else {
            stride = ((u32)(w + 3) >> 2) << 3;
        }
        return e + (stride * h + 8);
    }
    return ((void *)0);
}
