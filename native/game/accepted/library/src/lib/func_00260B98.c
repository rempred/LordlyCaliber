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

extern s32 D_80220CA4;
extern ResourceSlot *D_80220CA8;
extern s32 D_80220CAC;
extern s32 D_80220CB0;
extern ResourceSlot *D_80220CB8[];

ResourceSlot *func_00260B98(void)
{
    ResourceSlot *slot;

    if (D_80220CA4 == 0xFFFF) {
        /* Retail spins here when the free-list sentinel is reached. */
        for (;;) {
        }
    }
    slot = &D_80220CA8[D_80220CA4];
    D_80220CA4 = slot->next;
    if (++D_80220CB0 >= D_80220CAC) {
        D_80220CB0 = 0;
    }
    D_80220CB8[D_80220CB0] = slot;
    slot->data0C = slot;
    return slot;
}
