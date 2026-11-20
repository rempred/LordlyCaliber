typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
u16 D_8016B738(u16 a);
typedef struct { u16 id; u8 count; u8 unk3; } InvEntry;
extern InvEntry D_80193AC0[];
s32 func_000487ec(u8 id) {
    u16 slot = D_8016B738(id);
    if (slot < 40) {
        return D_80193AC0[slot].unk3 - D_80193AC0[slot].count;
    }
    return 0;
}
