typedef unsigned char u8;
typedef signed int s32;
u8 *func_002A2674(s32 a, s32 b);
u8 *func_002A3150(s32 a, s32 i, s32 b) {
    u8 *p = func_002A2674(a, b);
    if (p != ((void *)0)) {
        return (u8 *)(((s32 *)p)[i] + (s32)p);
    }
    return ((void *)0);
}
