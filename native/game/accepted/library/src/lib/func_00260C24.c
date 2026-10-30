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

extern ResourceSlot *func_0026061C(void *data);
extern void func_800712C4(void *data);
extern s32 D_80220CA4;
extern ResourceSlot *D_80220CA8;

void func_00260C24(void *data)
{
    ResourceSlot *slot;

    if (data != 0) {
        slot = func_0026061C(data);
        if (slot != 0 && --slot->unknown00 == 0) {
            slot->next = D_80220CA4;
            D_80220CA4 = slot - D_80220CA8;
            func_800712C4(slot->data0C);
            slot->data0C = 0;
            slot->unknown00 = 0;
            slot->code02 = 0;
            slot->key04 = 0;
            slot->key08 = 0;
        }
    } else {
        /* Retail does not return when passed a null pointer. */
        for (;;) {
        }
    }
}
