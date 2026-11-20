typedef unsigned char u8;
typedef struct {
    u8 unk00[0x7E];
    u8 step;
} Seq20F;
extern Seq20F *D_801D06E0;
void func_00211028(void) {
    if (D_801D06E0->step == 1) {
        D_801D06E0->step++;
    }
    if (D_801D06E0->step == 4) {
        D_801D06E0->step++;
    }
}
