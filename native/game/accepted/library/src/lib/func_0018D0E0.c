typedef signed int s32;
void *D_8017BFB0(s32 a, s32 b, void *c, s32 d, s32 e, s32 f, s32 g, s32 h, s32 i, s32 j, s32 k);
void func_0018D164(void);
extern s32 D_8021B5A4;
extern s32 D_8021B5B0;
extern void *D_8021B58C;
void func_0018D0E0(void) {
    if (D_8021B5A4 == -1) {
        D_8021B5A4 = D_8021B5B0;
        D_8021B58C = D_8017BFB0(0, 0, func_0018D164, 0x124, 0x18, 0x128, 0x1C, 0xAF, 0x18, 0x128, 0x4A);
    }
}
