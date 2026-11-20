typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
u8 *D_801CC410(s32 id);
void func_000b47d8(s16 *a, s16 *b, s16 *c, s16 *d);
void D_801CC6D4(u8 *w, void (*f)(), void (*g)(), s32 a, s32 b, s32 c, s32 d);
void D_801B7F50();
void D_801B74CC();
void func_000b8aec(void) {
    s16 a;
    s16 b;
    s16 c;
    s16 d;
    u8 *w = D_801CC410(0);
    if (w != ((void *)0)) {
        func_000b47d8(&a, &b, &c, &d);
        D_801CC6D4(w, D_801B7F50, D_801B74CC, a, b, c, d);
    }
}
