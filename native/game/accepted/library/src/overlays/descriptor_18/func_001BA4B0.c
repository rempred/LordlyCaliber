typedef unsigned short u16;
typedef signed int s32;
extern u16 *D_80229CE0[];
void func_001BA4B0(u16 bits) {
    s32 i;
    s32 j;
    for (i = 0; i < 3; i++) {
        u16 *p = D_80229CE0[i];
        for (j = 0; j < 4; j++) {
            *p &= ~bits;
        }
    }
}
