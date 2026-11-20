typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
u8 *D_801CC410(s32 id);
void func_000b4700(s16 *a, s16 *b, s16 *c, s16 *d);
void D_801CC784(u8 *w, s32 a, s32 b, s32 c, s32 d);
void func_000b8bf0(void) {
    u8 *w = D_801CC410(0);
    s16 a, b, c, d;
    if (w != ((void *)0)) {
        func_000b4700(&a, &b, &c, &d);
        D_801CC784(w, a, b, c, d);
    }
}
