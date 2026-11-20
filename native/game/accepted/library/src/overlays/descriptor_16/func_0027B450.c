typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
void *func_000016C4(void *p);
extern void *D_801CE8BC;
extern u8 *D_8022C5C0;
extern u8 D_8022C5C4;
extern u8 D_801976DA;
extern u8 D_800E9C0A[];
extern f32 D_801CE8EC;
extern f32 D_801CE8F0;
extern f32 D_801CE8F4;
void D_801BE894(void);
void D_800EA76C(void);
void D_80079618(s32 a);
void D_8020B098(void);
void D_801BBC78(void);
void D_801B0020(void);
void D_801C1D98(void);
void D_801C1A9C(void);
void D_801C234C(void);
void func_0027B450(void) {
    if (D_801CE8BC != ((void *)0)) {
        D_801BE894();
        func_0027FCF0();
        func_0027CDE0();
        if (D_8022C5C0 != ((void *)0)) {
            f32 x, y, z;
            D_801976DA = D_8022C5C0[0x14];
            x = *(f32 *)(D_8022C5C0 + 0x18);
            y = *(f32 *)(D_8022C5C0 + 0x1C);
            z = *(f32 *)(D_8022C5C0 + 0x20);
            D_800E9C0A[8] = D_8022C5C4;
            D_801CE8EC = x;
            D_801CE8F0 = y;
            D_801CE8F4 = z;
            func_000016C4(D_8022C5C0);
            D_8022C5C0 = ((void *)0);
        }
        D_800EA76C();
        D_80079618(-0x11);
        D_8020B098();
        D_801BBC78();
        D_801B0020();
        D_801C1D98();
        D_801C1A9C();
        D_801C234C();
        func_000016C4(D_801CE8BC);
        D_801CE8BC = ((void *)0);
    }
}
void func_0027CDE0(void);
void func_0027FCF0(void);
