typedef unsigned char u8;
typedef signed int s32;
extern u8 D_800EB0B0[];
extern s32 D_8022A8A8[];
void D_800EA838(void *p);
void D_800EA884(s32 a);
void func_002831E0(s32 i) {
    if (i == 7) {
        D_800EA838(D_800EB0B0);
    } else {
        D_800EA884(D_8022A8A8[i]);
    }
}
