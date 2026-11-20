typedef unsigned char u8;
typedef signed int s32;
extern u8 D_80196AEC;
extern u8 D_801936E0;
extern s32 D_801971E8;
extern s32 D_80193674;
void func_00049574(u8 mode) {
    D_801936E0 = mode;
    if (mode == 1) {
        if (!(D_80196AEC & 2)) {
            D_801971E8 = 0;
        }
    } else if (mode == 2) {
        if (!(D_80196AEC & 4)) {
            D_80193674 = 0;
        }
    }
}
