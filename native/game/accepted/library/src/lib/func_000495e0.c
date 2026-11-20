typedef unsigned char u8;
extern u8 D_801936E0;
extern u8 D_8018F330[];
extern u8 D_8018F344[];
u8 *func_000495e0(void) {
    u8 *p = D_8018F330;
    if (D_801936E0) {
        p = D_8018F344;
    }
    return p;
}
