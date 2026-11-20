typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
extern u8 D_8023E5FF[];
s32 func_0028a814(s16 n) {
    return (D_8023E5FF[n / 8] >> (n % 8)) & 1;
}
