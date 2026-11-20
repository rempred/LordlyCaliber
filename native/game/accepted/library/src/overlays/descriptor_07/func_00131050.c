typedef signed int s32;
typedef unsigned int u32;
void D_801B2B78(u32 *unit);
extern u32 *D_801F0CB0[];
void func_00131050(void) {
    s32 i;
    for (i = 0; i < 50; i++) {
        D_801B2B78(D_801F0CB0[i]);
    }
}
