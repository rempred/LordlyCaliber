typedef unsigned char u8;
typedef signed short s16;
extern s16 D_8022A71E;
extern u8 D_8022A821;
extern u8 D_8022A824;
extern s16 D_8022A826;
void func_001C2938(void) {
    if (D_8022A821 == 0) {
        return;
    }
    if (D_8022A821 == 1) {
        D_8022A826 += D_8022A71E / D_8022A824;
        if (D_8022A826 >= D_8022A71E) {
            D_8022A826 = D_8022A71E;
        }
    } else if (D_8022A821 == 2) {
        D_8022A826 -= D_8022A71E / D_8022A824;
        if (D_8022A826 < 0) {
            D_8022A826 = 0;
            D_8022A821 = 0;
        }
    }
}
