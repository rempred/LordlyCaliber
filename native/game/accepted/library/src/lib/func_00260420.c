#include "common/types.h"

typedef struct ResourceSlot {
    u16 unknown00;
    u16 code02;
    s32 key04;
    s32 key08;
    void *data0C;
    u16 next;
    u16 unknown12;
} ResourceSlot;

extern s32 D_80220CA0;
extern s32 D_80220CA4;
extern ResourceSlot *D_80220CA8;
extern s32 D_80220CAC;
extern s32 D_80220CB0;
extern ResourceSlot *D_80220CB8[];
extern void *func_80070F30(s32 size);
extern void func_80093380(void *destination, s32 size);

void func_00260420(void)
{
    s32 i;

    D_80220CA0 = 128;
    D_80220CA8 = func_80070F30(2560);
    func_80093380(D_80220CA8, 2560);
    for (i = 0; i < D_80220CA0 - 1; i++) {
        D_80220CA8[i].next = i + 1;
    }
    D_80220CAC = 32;
    D_80220CA4 = 0;
    D_80220CB0 = 0;
    D_80220CA8[i].next = 0xFFFF;
    func_80093380(D_80220CB8, 128);
}
