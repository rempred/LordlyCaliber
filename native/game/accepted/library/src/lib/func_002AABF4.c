typedef unsigned char u8;
typedef signed int s32;
extern u8 *D_8022A974;
void *func_00001330(s32 size);
u8 *func_002AABF4(void) {
    u8 *p = (*(u8 * *)((u8 *)(D_8022A974) + (0x1CA8)));
    u8 i;
    if (p == ((void *)0)) {
        p = func_00001330(0x1D);
        (*(u8 * *)((u8 *)(D_8022A974) + (0x1CA8))) = p;
        for (i = 0; i < 28; i++) {
            p[i] = 4;
        }
    }
    return p;
}
