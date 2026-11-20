typedef unsigned char u8;
typedef signed short s16;
extern u8 D_8023DF18[];
extern u8 D_8023E5D0[];
void func_0028a84c(u8 id, u8 val) {
    s16 i;
    for (i = 0; D_8023DF18[i] != 0xFF; i++) {
        if (D_8023DF18[i] == id) {
            D_8023E5D0[i] = val;
            return;
        }
    }
}
