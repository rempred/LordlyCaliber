typedef unsigned char u8;
typedef unsigned short u16;
u16 D_8016B6FC(u16 a);
typedef struct { u16 id; u8 count; u8 unk3; } InvEntry;
extern InvEntry D_80193AC0[];
extern u8 D_801936A6;
static u8 func_0004890C(u8 i);
u8 func_000488cc(u16 id) {
    u16 slot = D_8016B6FC(id);
    if (slot < 278) {
        return D_80193AC0[slot].unk3;
    }
    return 0;
}
static u8 func_0004890C(u8 i) {
    return (D_801936A6 >> (i * 2)) & 3;
}
