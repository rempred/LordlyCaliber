typedef unsigned char u8;
typedef signed int s32;
extern void *D_8021B170;
extern void *D_8021B174;
extern s32 D_8021B2AC;
extern s32 D_8021B2A4;
extern u8 D_80210219;
void D_8017C384(void *w);
extern void *D_8021B174;
extern s32 D_8021B2AC;
extern s32 D_8021B2A4;
extern u8 D_80210219;
void D_8017C384(void *w);
void func_0018F418(void) {
    s32 pad[4];
    if (D_8021B170 != ((void *)0)) {
        D_8017C384(D_8021B170);
    }
    if (D_8021B2AC != -1) {
        D_8017C384(D_8021B174);
    }
    D_8021B2A4 = 0;
    D_80210219 = 0x11;
}
