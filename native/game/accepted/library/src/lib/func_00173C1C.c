typedef unsigned char u8;
typedef signed int s32;
extern u8 D_80214E84;
extern u8 D_801F365D;
extern s32 D_801F0DE0;
extern s32 D_801F0DE8;
void func_00173C1C(void) {
    if (D_80214E84 == 0) {
        D_801F0DE0 = 0;
        D_801F0DE8 = 0;
        D_801F365D &= 0xFE;
    }
}
