typedef unsigned char u8;
typedef unsigned int u32;
extern u8 D_801CEEC0[][4];
u8 func_00201D50(u32 a, u32 b) {
    if (a >= 5 || b >= 4) {
        return 0;
    }
    return D_801CEEC0[a][b];
}
