typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
u16 D_801794C8(s32 a);
void D_8017C4B4(s16 a);
void D_80179034(s32 a, s32 x, s32 y);
void func_001F3714(u8 *o, s32 i) {
    u16 w = D_801794C8((*(s32 *)((u8 *)((*(u8 * *)((u8 *)(o) + (0xD0)))) + (0x530))));
    s32 x;
    x = (270 - w) / 2;
    D_8017C4B4(((s16 *)(o + 0x18))[i]);
    D_80179034((*(s32 *)((u8 *)((*(u8 * *)((u8 *)(o) + (0xD0)))) + (0x530))), (*(s32 *)((u8 *)(o) + (0x2C))) + x, (*(s32 *)((u8 *)(o) + (0x30))) + 1);
}
