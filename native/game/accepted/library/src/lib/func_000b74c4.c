typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
extern u8 *D_80196AF8;
extern u16 D_80190EBC[];
extern u8 D_80191010[];
u16 func_000b86f8(u8 i);
void func_000b74c4(void) {
    u8 *slots = D_80196AF8 + 0x17D0;
    s32 i;
    u8 u;
    u8 *p;
    for (i = 0; i < 9; i++) {
        u = (slots + i)[4];
        if (u == 0xFF) {
            continue;
        }
        p = D_80196AF8 + (u * 2 + 0x1872);
        if (u < 100) {
            if (*p & 2) {
                *p &= ~2;
            }
        } else {
            if (*p & 2) {
                *p &= ~0x42;
            }
            ((u8 *)D_80190EBC)[0xC5] += func_000b86f8(u);
            D_80190EBC[u] = 0;
            D_80191010[u] = 0;
        }
    }
}
