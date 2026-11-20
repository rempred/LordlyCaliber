typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
extern u8 D_801936A9;
void func_00045cb0(s8 delta) {
    u8 *p = &D_801936A9;
    s16 v = *p;
    v += delta;
    if (v < 0) {
        *p = 0;
    } else if (v > 100) {
        *p = 100;
    } else {
        *p = v;
    }
}
