typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
extern u8 D_8018FC19;
extern f32 D_8022A720;
extern f32 D_8022A734;
extern f32 D_8022A738;
extern f32 D_8022A73C;
extern f32 D_8022A740;
extern f32 D_8022A744;
extern f32 D_8022A748;
extern f32 D_801D0700;
extern f32 D_801D0704;
extern f32 D_801D0710;
extern f32 D_801D0720;
extern f32 D_801D072C;
extern f32 D_801D0730;
extern f32 D_801D083C;
void func_0029A4C0(s32 *in) {
    f32 v[7];
    s32 i;
    for (i = 0; i < 7; i++) {
        v[i] = *in++ / 1000.0f;
    }
    D_8022A740 = v[0];
    D_8022A744 = v[1];
    D_8022A748 = v[2];
    D_8022A734 = v[3];
    D_8022A738 = v[4];
    D_8022A73C = v[5];
    D_8022A720 = v[6];
    if (D_8018FC19 == 2) {
        D_801D0710 = v[6];
        D_801D0720 = v[3];
        D_801D072C = v[4];
        D_801D0704 = v[5];
        D_801D0730 = v[0];
        D_801D083C = v[1];
        D_801D0700 = v[2];
    }
}
