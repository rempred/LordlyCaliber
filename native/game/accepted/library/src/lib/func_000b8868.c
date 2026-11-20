typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80196AF8;
void D_801AA2B8();
s32 func_000b93d4(u16 kind, u8 b, void (*fn)(), s16 x, s16 y, s16 w, s16 h, s16 x2, s16 y2, s16 w2, s16 h2);
void func_000b8868(u8 *o) {
    s32 x = (*(s32 *)((u8 *)(o) + (0x24)));
    s32 y = (*(s32 *)((u8 *)(o) + (0x28)));
    s16 r = func_000b93d4(4, 0x12, D_801AA2B8, x - 12, y - 8, x - 4, y + 8, x - 0x3E, y - 0x10,
                                     x - 4, y + 0x10);
    u8 *st = D_80196AF8;
    st += r * 0x108;
    st[0x737] = o[0xE7];
}
