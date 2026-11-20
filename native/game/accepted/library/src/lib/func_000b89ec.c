typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
u8 *D_801CC410(s32 id);
void D_801CC684(u8 *w, void (*f)(), s32 a, s32 b, s32 c, s32 d);
s32 func_000b93d4(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2);
void D_801B4AF4();
void D_801B577C();
void func_000b89ec(void) {
    u8 *w = D_801CC410(0);
    if (w != ((void *)0)) {
        D_801CC684(w, D_801B4AF4, (s16)(*(s32 *)((u8 *)(w) + (0xC))), (s16)(*(s32 *)((u8 *)(w) + (0x10))),
                                 (s16)((*(s32 *)((u8 *)(w) + (0xC))) + 0x82), (s16)((*(s32 *)((u8 *)(w) + (0x10))) + 0x30));
        func_000b93d4(2, 10, D_801B577C, 0x120, 0x14, 0x128, 0x1C, 0xA6, 0x14, 0x128, 0x44);
    }
}
