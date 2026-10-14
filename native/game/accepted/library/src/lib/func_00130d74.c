typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef int s32;

extern u8 D_801951B0[];
extern u8 D_8018F481;
extern u8 D_801E7E13[], D_801E7E14[];
extern s32 func_0005c110(s32);
extern s32 func_00045df4(s16);

void func_00130D74(u16 site, s8 *out0, u8 *out1)
{
    u16 flags;

    flags = *(u16 *)(D_801951B0 + site * 36 + 28);
    if (flags & 2) {
        *out0 = D_801E7E13[D_8018F481 * 12];
        if (func_0005c110(0x11)) {
            if (func_00045df4(0x6B)) {
                *out0 = 1;
            } else {
                *out0 = 4;
            }
        }
        *out1 = 1;
    } else if (flags & 4) {
        *out0 = D_801E7E14[D_8018F481 * 12];
        *out1 = 2;
    } else {
        *out0 = -1;
        *out1 = 0;
    }
}
