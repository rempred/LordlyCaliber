typedef unsigned char u8;
typedef signed int s32;
u8 *D_801CC410(s32 id);
void D_801CC684(u8 *w, void (*f)(), s32 a, s32 b, s32 c, s32 d);
void D_801B6394();
void func_000b893c(void) {
    D_801CC684(D_801CC410(0), D_801B6394, 0x18, 0x14, 0x128, 0x8A);
}
