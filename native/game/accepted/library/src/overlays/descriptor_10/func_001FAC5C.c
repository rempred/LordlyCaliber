typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 D_801969B0[];
void func_001F962C(void *a, void *b, u8 *img, u8 c, s32 d, s32 e);
void func_001F9E00(void *a, void *b, u8 *img, u8 c, s32 d, s32 e);
void func_001FA240(void *a, void *b, u8 *img);
void func_001FA714(u8 *img, s32 x, s32 y, s32 s, void *b, void *m, void *a);
void func_001FAC5C(void *a, void *b, u8 *img, u8 c, s32 d, s32 e) {
    if (*(u16 *)img == 0x3634) {
        func_001FA714(img, -160, -120, 2, b, D_801969B0, a);
    } else if (img[2] == 1) {
        func_001FA240(a, b, img);
    } else if (img[2] == 3) {
        func_001F9E00(a, b, img, c, d, e);
    } else {
        func_001F962C(a, b, img, c, d, e);
    }
}
