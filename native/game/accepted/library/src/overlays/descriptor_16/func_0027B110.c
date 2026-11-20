typedef signed char s8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
void func_0027B110(void);
extern s32 D_800C6D60;
extern s8 *D_800C47F0[];
extern u16 D_800E9BE4[];
void func_0027B110(void) {
    s16 i;
    s8 y;
    for (i = 0; i < D_800C6D60; i++) {
        y = D_800C47F0[i][3];
        if (y < -15) {
            D_800E9BE4[i] = (D_800E9BE4[i] & ~0x800) | 0x400;
        } else if (y >= 26) {
            D_800E9BE4[i] = (D_800E9BE4[i] & ~0x400) | 0x800;
        }
    }
}
