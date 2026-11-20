typedef unsigned char u8;
typedef struct {
    u8 unk00[0x7E];
    u8 step;
} Seq1ED;
extern Seq1ED *D_801BA6D0;
void func_001ED888(void) {
    if (D_801BA6D0->step == 1) {
        D_801BA6D0->step++;
    }
    if (D_801BA6D0->step == 4) {
        D_801BA6D0->step++;
    }
}
