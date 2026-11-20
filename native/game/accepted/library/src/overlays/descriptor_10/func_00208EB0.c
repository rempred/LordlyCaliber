typedef unsigned char u8;
typedef signed int s32;
extern s32 D_801D06B0[8];
extern u8 D_800EB0B0[];
extern u8 D_800EB100[];
extern u8 D_800EB150[];
extern u8 D_800EB1A0[];
extern u8 D_800EB1F0[];
extern u8 g_func_0015DF10_match[];
extern u8 g_func_0015DF10_default[];
void D_800EA6B0(void *ch, s32 a, s32 b);
void D_800EA604(void *ch, s32 sound);
void func_00208EB0(void) {
    if (D_801D06B0[0] >= 0) {
        D_800EA6B0(D_800EB0B0, D_801D06B0[0], 10);
    }
    if (D_801D06B0[1] >= 0) {
        D_800EA6B0(D_800EB100, D_801D06B0[1], 10);
    }
    if (D_801D06B0[2] >= 0) {
        D_800EA604(D_800EB150, D_801D06B0[2]);
    }
    if (D_801D06B0[3] >= 0) {
        D_800EA604(D_800EB1A0, D_801D06B0[3]);
    }
    if (D_801D06B0[4] >= 0) {
        D_800EA604(D_800EB1F0, D_801D06B0[4]);
    }
    if (D_801D06B0[5] >= 0) {
        D_800EA604(g_func_0015DF10_match, D_801D06B0[5]);
    }
    if (D_801D06B0[6] >= 0) {
        D_800EA604(g_func_0015DF10_default, D_801D06B0[6]);
    }
}
