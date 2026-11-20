typedef signed int s32;
extern void *D_8021B588;
void *D_8017BFB0(s32 a, s32 b, void *c, s32 d, s32 e, s32 f, s32 g, s32 h, s32 i, s32 j, s32 k);
void func_0018D164(void);
void func_0018D06C(void) {
    void **p = &D_8021B588;
    if (*p == ((void *)0)) {
        *p = D_8017BFB0(0, 0, func_0018D164, 0x18, 0x18, 0x1C, 0x1C, 0x18, 0x18, 0x91, 0x4A);
    }
}
