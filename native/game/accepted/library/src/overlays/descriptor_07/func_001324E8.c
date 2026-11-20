typedef signed int s32;
extern s32 D_801EB280;
extern s32 D_801EB284;
void D_8017C384(s32 h);
void func_001324E8(void) {
    s32 *p = &D_801EB280;
    if (*p != 0) {
        D_8017C384(*p);
        *p = 0;
    }
    D_8017C384(D_801EB284);
    D_801EB284 = 0;
}
