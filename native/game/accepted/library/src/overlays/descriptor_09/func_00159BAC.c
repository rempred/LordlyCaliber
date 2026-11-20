typedef unsigned char u8;
typedef signed short s16;
void func_00159BAC(u8 *a, s16 n) {
    s16 i;
    s16 j;
    u8 t;
    for (i = 1; i < n; i++) {
        t = a[i];
        for (j = i - 1; j >= 0 && a[j] > t; j--) {
            a[j + 1] = a[j];
        }
        a[j + 1] = t;
    }
}
