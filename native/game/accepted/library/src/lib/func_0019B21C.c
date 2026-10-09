typedef unsigned char u8;
typedef signed char s8;
typedef signed int s32;

extern s8 D_8021A11A[];
extern u8 D_8021A114[];

u8 func_0019B21C(u8 category_index)
{
    s8 count = D_8021A11A[category_index];
    u8 total = 0;
    s32 index;

    for (index = 0; index < count; index++) {
        total += D_8021A114[index];
    }
    return total;
}
