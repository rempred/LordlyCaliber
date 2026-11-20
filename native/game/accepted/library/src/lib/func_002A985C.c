typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 *D_8022A974;
u8 D_801C1AA4(s32 a, s32 b, u8 c, s16 d, s16 e, s32 f, u8 *g, u8 *h, u8 *i);
u8 func_002A985C(s32 i) {
    u8 b[3];
    u8 *o = (*(u8 * *)((u8 *)(D_8022A974 + (i) * 4) + (0x18)));
    return D_801C1AA4((*(s32 *)((u8 *)(o) + (0xE8))), (*(s32 *)((u8 *)(o) + (0xEC))), o[0x146], (*(s16 *)((u8 *)(o) + (0x13A))), (*(s16 *)((u8 *)(o) + (0x134))),
                         (*(s32 *)((u8 *)(o) + (0xF0))), &b[0], &b[1], &b[2]);
}
