typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
u8 *D_80227A18(s32 id, s32 n);
u32 func_0028ac88(s32 siz, u32 w);
u8 *func_00291130(u8 **out) {
    u8 *t = D_80227A18(0x18855FE, 8);
    *out = t + (func_0028ac88(t[3], *(u16 *)(t + 4)) * *(u16 *)(t + 6) + 8);
    return t;
}
