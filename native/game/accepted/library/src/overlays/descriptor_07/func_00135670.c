typedef signed int s32;
extern s32 D_801ED4A4[];
s32 func_00135670(s32 a) {
    s32 *p = D_801ED4A4;
    s32 i = 0;
    while (*p != a) {
        p += 3;
        i += 3;
    }
    return i;
}
