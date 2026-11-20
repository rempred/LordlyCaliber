typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
extern s16 D_801D07F4;
extern s16 D_801D07F6;
extern s16 D_801D07F8;
extern s16 D_801D07FA;
extern s16 D_801D07FC;
extern s16 D_801D07FE;
extern s16 D_801D0800;
extern s16 D_801D0802;
extern s16 D_801D0804;
extern s16 D_801D0806;
extern s16 D_801D0808;
extern s16 D_801D080A;
extern u8 D_801D080C;
extern u8 D_801D080D;
void func_001FF8E0(void) {
    u32 temp_t4;
    if (D_801D080C != 0) {
        temp_t4 = D_801D080D & 0xFF;
        if ((u8) D_801D080C >= temp_t4) {
            D_801D080D += 1;
            D_801D07F4 = D_801D07FC + ((s32) ((D_801D0804 - D_801D07FC) * temp_t4) / (s32) D_801D080C);
            D_801D07F6 = D_801D07FE + ((s32) ((D_801D0806 - D_801D07FE) * temp_t4) / (s32) D_801D080C);
            D_801D07F8 = D_801D0800 + ((s32) ((D_801D0808 - D_801D0800) * temp_t4) / (s32) D_801D080C);
            D_801D07FA = D_801D0802 + ((s32) ((D_801D080A - D_801D0802) * temp_t4) / (s32) D_801D080C);
        }
    }
}
