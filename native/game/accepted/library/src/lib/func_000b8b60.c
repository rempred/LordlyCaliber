typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
u8 *D_801CC410(s32 id);
void D_801B3B00();
void func_000b4700(s16 *a, s16 *b, s16 *c, s16 *d);
void D_801B74CC();
void D_801CC72C(u8 *w, void (*f)(), void (*g)(), s32 a, s32 b, s32 c, s32 d);
void D_801CC5B4(u8 *w, s32 a, s32 b, s32 c, s32 d);
void func_000b8b60(void) {
    s16 a;
    s16 b;
    s16 c;
    s16 d;
    u8 *w = D_801CC410(0);
    if (w != ((void *)0)) {
        func_000b4700(&a, &b, &c, &d);
        D_801CC72C(w, D_801B3B00, D_801B74CC, a, b, c, d);
        D_801CC5B4(w, a, b, c, d);
    }
}
