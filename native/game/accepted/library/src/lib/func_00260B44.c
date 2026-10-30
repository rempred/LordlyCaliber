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

void func_00260B44(void *data)
{
    ResourceSlot *slot = D_80220CA8;
    s32 i;

    for (i = 0; i < D_80220CA0; i++, slot++) {
        if (slot->data0C == data) {
            slot->unknown00++;
            return;
        }
    }
}
