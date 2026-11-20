typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80196AF8;
u8 *D_801CC410(s32 id);
s32 func_000b93d4(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2);
void func_000016C4(void *p);
s32 D_801CC3B8(s32 id);
void D_801C0798(s16 a, s32 b);
void D_801A43F4();
void func_000b4b08(s32 a) {
    u8 *w = D_801CC410(5);
    u8 *x = w;
    if (w == ((void *)0)) {
        (*(s32 *)((u8 *)(D_80196AF8) + (0x10D0))) = func_000b93d4(5, 10, D_801A43F4, 0x9C, 0xD6, 0xA4, 0xDE, 0x18, 0xB6, 0x128, 0xDE);
    } else {
        func_000016C4((*(void * *)((u8 *)(x) + (0xF4))));
    }
    D_801C0798(D_801CC3B8(5), a);
}
