typedef signed short s16;
typedef signed int s32;
void D_801B3B00();
void func_000b4700(s16 *a, s16 *b, s16 *c, s16 *d);
void func_000b94b0(s32 a, s32 b, void (*f)(), s32 c, s32 d, s32 e, s32 g);
void func_000b8a94(void) {
    s16 a, b, c, d;
    func_000b4700(&a, &b, &c, &d);
    func_000b94b0(0, 10, D_801B3B00, a, b, c, d);
}
