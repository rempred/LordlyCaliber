typedef unsigned char u8;
typedef signed int s32;
extern s32 D_801F3670;
extern u8 D_801EB2D4;
extern u8 D_801EB2D5;
void D_80079618();
void func_00131C70(void) {
    if (D_801F3670 != 0) {
        D_80079618(0x218E9B4, D_801EB2D4, -0x11);
        D_801F3670 = 0;
        D_801EB2D4 = 0xFF;
        D_801EB2D5 = 0;
    }
}
