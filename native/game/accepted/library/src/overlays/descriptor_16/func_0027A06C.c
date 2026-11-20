typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef struct {
    u8 unk00[0x4C];
    s32 w4C;
} Unit27;
extern Unit27 *D_8022D134[];
extern s32 D_8022D150[];
s32 D_8016E1DC(s32 a, s32 b);
s32 func_0027A06C(s16 i) {
    if (D_8022D134[i]->w4C == 1) {
        return D_8016E1DC(1, 1);
    }
    return D_8022D150[i];
}
