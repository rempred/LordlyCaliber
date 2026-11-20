typedef unsigned char u8;
typedef signed int s32;
extern u8 D_80197168[];
void func_0005c090(s32 flag, s32 value) {
    if (value) {
        D_80197168[flag / 8] |= 1 << (flag % 8);
    } else {
        D_80197168[flag / 8] &= ~(1 << (flag % 8));
    }
}
