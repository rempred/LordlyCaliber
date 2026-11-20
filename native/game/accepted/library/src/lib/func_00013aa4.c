typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
s32 func_00013aa4(s32 range);
extern s32 D_800B1824;
s32 func_00013aa4(s32 range) {
    s32 i;
    for (i = 0; i < 8; i++) {
        u32 t = D_800B1824 & 0x48000000;
        D_800B1824 <<= 1;
        if (t == 0x48000000 || t == 0x08000000) {
            D_800B1824 |= 1;
        }
    }
    return (f32)range * ((f32)D_800B1824 / 65536.0f / 65536.0f);
}
