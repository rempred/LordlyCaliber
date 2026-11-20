typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
u16 D_8016B6FC(u16 a);
typedef struct { u16 id; u8 count; u8 unk3; } InvEntry;
extern InvEntry D_80196B00[];
s32 func_0004883c(u16 id) {
    u16 slot = D_8016B6FC(id);
    if (slot < 278) {
        return D_80196B00[slot].unk3 - D_80196B00[slot].count;
    }
    return 0;
}
