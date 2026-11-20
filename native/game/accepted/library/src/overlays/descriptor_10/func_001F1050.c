typedef unsigned char u8;
typedef signed int s32;
typedef struct {
    u8 unk0000[0x1C0];
    s32 strategy;
    u8 unk01C4[0x5020];
    s32 listB[0x36];
    s32 nB;
    s32 listA[0x100];
    s32 nA;
} Ctx1F;
extern Ctx1F *D_801CE8BC;
void func_001F1050(s32 v) {
    s32 i;
    for (i = 0; i < D_801CE8BC->nB; i++) {
        if (D_801CE8BC->listB[i] == v) {
            bcopy(&D_801CE8BC->listB[i + 1], &D_801CE8BC->listB[i], (D_801CE8BC->nB - i - 1) * 4);
            D_801CE8BC->nB--;
            D_801CE8BC->listB[D_801CE8BC->nB] = 0;
            return;
        }
    }
}
