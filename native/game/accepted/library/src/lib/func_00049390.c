typedef unsigned char u8;
typedef signed int s32;
extern u8 D_8019367C[];
s32 func_00049390(u8 v) {
    u8 i;
    for (i = 0; i < 5; i++) {
        if (D_8019367C[i] == v) {
            return 1;
        }
    }
    return 0;
}
