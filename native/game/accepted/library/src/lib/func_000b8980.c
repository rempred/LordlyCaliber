typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
u8 *D_801CC410(s32 id);
void D_801CC7D4(s32 a);
void D_801CC684(u8 *w, void (*f)(), s32 a, s32 b, s32 c, s32 d);
void D_801B3B00();
void func_000b4700(s16 *a, s16 *b, s16 *c, s16 *d);
void func_000b8980(void) {
    s16 a;
    s16 b;
    s16 c;
    s16 d;
    u8 *w = D_801CC410(0);
    func_000b4700(&a, &b, &c, &d);
    D_801CC7D4(2);
    D_801CC684(w, D_801B3B00, a, b, c, d);
}
