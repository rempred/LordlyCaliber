extern const unsigned char g_func_0015DF10_match[];
extern const unsigned char g_func_0015DF10_default[];
extern unsigned char D_800EB0B0[];
extern unsigned char D_800EB100[];
extern unsigned char D_800EB150[];
extern unsigned char D_800EB1A0[];
extern unsigned short D_800E810E;
extern void func_800EA604(void *arg0);

void func_00208DC8(void *arg0, int arg1)
{
    if (arg0 == g_func_0015DF10_match) {
        *(int *)0x801D06C4 = arg1;
    } else if (arg0 == g_func_0015DF10_default) {
        *(int *)0x801D06C8 = arg1;
    } else if (arg0 == 0) {
        if (D_800E810E != 12) {
            func_800EA604(0);
        }
    } else if (arg0 == D_800EB0B0) {
        *(int *)0x801D06B0 = arg1;
    } else if (arg0 == D_800EB100) {
        *(int *)0x801D06B4 = arg1;
    } else if (arg0 == D_800EB150) {
        *(int *)0x801D06B8 = arg1;
    } else if (arg0 == D_800EB1A0) {
        *(int *)0x801D06BC = arg1;
    } else {
        *(int *)0x801D06C0 = arg1;
    }
}
