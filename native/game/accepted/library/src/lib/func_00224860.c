typedef unsigned char u8;
typedef signed int s32;
extern u8 D_801976E8[];
u8 *func_002247CC(s32 enemy);
extern inline u8 *func_00224790(s32 side) {
    u8 *p = D_801976E8;
    if (!side) {
        p -= 12;
    }
    if (p[1] & 1) {
        return func_002247CC(side);
    }
    return ((void *)0);
}
u8 func_00224860(s32 side) {
    u8 *r = func_00224790(side);
    if (r != ((void *)0)) {
        return r[8];
    }
    return 0xFF;
}
