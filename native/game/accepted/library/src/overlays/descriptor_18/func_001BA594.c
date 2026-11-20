typedef unsigned short u16;
typedef signed int s32;
extern u16 *D_80229D04[];
void func_001BA594(u16 bits) {
    s32 i;
    s32 j;
    for (i = 0; i < 3; i++) {
        u16 *p = D_80229D04[i];
        for (j = 0; j < 4; j++) {
            *p &= ~bits;
        }
    }
}
