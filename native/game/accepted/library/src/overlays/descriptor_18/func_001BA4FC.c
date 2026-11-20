typedef unsigned short u16;
typedef signed int s32;
extern u16 *D_80229CEC[];
void func_001BA4FC(u16 bits) {
    s32 i;
    s32 j;
    for (i = 0; i < 3; i++) {
        u16 *p = D_80229CEC[i];
        for (j = 0; j < 4; j++) {
            *p &= ~bits;
        }
    }
}
