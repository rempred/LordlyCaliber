typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    s32 w0;
    u8 b[4];
    s32 w8;
    s32 wC;
} Rec151_16;
void func_00159C4C(Rec151_16 *a, s16 n) {
    s16 i;
    s16 j;
    Rec151_16 t;
    u8 k;
    for (i = 1; i < n; i++) {
        k = a[i].b[3];
        t = a[i];
        for (j = i - 1; j >= 0 && a[j].b[3] > k; j--) {
            a[j + 1] = a[j];
        }
        a[j + 1] = t;
    }
}
