typedef unsigned char u8;
typedef unsigned short u16;
u16 D_8016B738(u16 a);
typedef struct { u16 id; u8 count; u8 unk3; } InvEntry;
extern InvEntry D_80193AC0[];
u8 func_0004888c(u16 id) {
    u16 slot = D_8016B738(id);
    if (slot < 40) {
        return D_80193AC0[slot].unk3;
    }
    return 0;
}
