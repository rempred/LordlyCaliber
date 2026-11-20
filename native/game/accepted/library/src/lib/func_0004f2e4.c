typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
extern s16 D_80190F74;
extern u8 *D_8018FC70;
extern u8 *D_8018FC74;
s32 func_00051450(u8 c);
u8 func_0004f2e4(u8 c, s32 alt) {
    u8 *p;
    if (c < 0x11) {
        return 0;
    }
    if (func_00051450(c)) {
        return 0xB;
    }
    if (D_80190F74 != -1) {
        if (!alt) {
            p = D_8018FC70 + D_80190F74;
        } else {
            p = D_8018FC74 + D_80190F74;
        }
    } else if (!alt) {
        if (c >= 0x80) {
            p = D_8018FC70 + c;
        } else {
            p = (u8 *)((u32)c + (u32)D_8018FC70);
            return p[0x10];
        }
    } else {
        if (c < 0x80) {
            p = (u8 *)((u32)c + (u32)D_8018FC74);
            return p[0x10];
        }
        p = D_8018FC74 + c;
    }
    return *p;
}
s32 func_00051450(u8 c);
