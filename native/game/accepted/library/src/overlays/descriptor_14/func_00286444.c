typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
extern s8 *D_800C47F0;
s32 func_00286444(void) {
    f32 x = 0.0f;
    f32 y = x;
    if (D_800C47F0 != ((void *)0)) {
        if ((u8)(D_800C47F0[2] + 4) >= 9) {
            x = D_800C47F0[2] / 16.0f;
        }
        if ((u8)(D_800C47F0[3] + 4) >= 9) {
            y = D_800C47F0[3] / 16.0f;
        }
    }
    return (x + y) != 0.0;
}
