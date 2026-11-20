typedef unsigned char u8;
extern u8 *D_8022A974;
u8 func_002A92E8(void) {
    u8 id;
    u8 j;
    u8 *o;
    for (id = 0; id < 28; id++) {
        for (j = 0; j < 28; j++) {
            o = (*(u8 * *)((u8 *)(D_8022A974 + (j) * 4) + (0x18)));
            if (o != ((void *)0) && o[0x147] == id) {
                break;
            }
        }
        if (j >= 28) {
            return id;
        }
    }
    return 0xFF;
}
