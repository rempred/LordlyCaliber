typedef unsigned char u8;
extern u8 D_801936E0;
extern u8 D_8018FD64[];
extern u8 D_8018FD78[];
u8 *func_000514bc(void) {
    u8 *p = D_8018FD64;
    if (D_801936E0) {
        p = D_8018FD78;
    }
    return p;
}
