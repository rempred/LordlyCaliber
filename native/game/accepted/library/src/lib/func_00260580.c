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
extern ResourceSlot *D_80220CA8;
extern void func_800712C4(void *pointer);
extern void func_80093380(void *destination, s32 size);

void func_00260580(void)
{
    s32 i;

    for (i = 0; i < D_80220CA0; i++) {
        if (D_80220CA8[i].data0C != 0) {
            func_800712C4(D_80220CA8[i].data0C);
        }
    }
    func_80093380(D_80220CA8, D_80220CA0 * sizeof(ResourceSlot));
    func_800712C4(D_80220CA8);
}
