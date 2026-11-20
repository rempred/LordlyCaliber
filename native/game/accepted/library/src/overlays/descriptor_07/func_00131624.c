typedef unsigned char u8;
typedef signed int s32;
extern s32 D_801F0DE0;
extern s32 D_801F3610;
extern u8 D_801F365D;
s32 func_00131624(void) {
    s32 r = D_801F0DE0 == 0;
    if (D_801F3610 != 0) {
        r = 0;
    }
    if (D_801F365D & 2) {
        r = 0;
    }
    return r;
}
