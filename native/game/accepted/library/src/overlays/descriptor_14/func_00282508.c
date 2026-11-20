typedef unsigned char u8;
typedef signed short s16;
extern u8 D_800E7A32;
extern u8 *D_8018FC10;
void func_00282508(void);
void func_00282508(void) {
    u8 *f = D_8018FC10;
    if (f == ((void *)0)) {
        return;
    }
    if (f[7] == 1) {
        u8 *flags = &D_800E7A32;
        *flags |= 2;
        return;
    }
    if (f[7] == 2) {
        f[7] = 0;
    }
    if (*(s16 *)f > 0) {
        s16 n = *(s16 *)f - 1;
        f[8] = f[9] + (f[0xA] - f[9]) * n / *(s16 *)(f + 2);
        *(s16 *)f = n;
    }
}
