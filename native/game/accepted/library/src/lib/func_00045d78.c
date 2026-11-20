typedef unsigned char u8;
typedef signed int s32;
extern u8 D_80193686[];
void func_00045d78(s32 flag, s32 value) {
    if (value == 1) {
        D_80193686[flag / 8] |= 1 << (flag % 8);
    } else {
        D_80193686[flag / 8] &= ~(1 << (flag % 8));
    }
}
