typedef unsigned char u8;
typedef signed int s32;
extern u8 D_80197AE7[];
s32 func_0005c140(s32 flag) {
    if (flag == -1) {
        return 1;
    }
    return ((D_80197AE7[flag / 8] >> (flag % 8)) & 1) ^ 1;
}
