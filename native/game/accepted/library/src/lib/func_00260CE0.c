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

extern ResourceSlot *func_00260828(s32 value, s32 key04, s32 key08);
extern void func_800712C4(void *data);
extern s32 D_80220CA4;
extern ResourceSlot *D_80220CA8;

void func_00260CE0(u16 code, s32 key04, s32 key08)
{
    ResourceSlot *slot = func_00260828(code, key04, key08);

    if (slot != 0) {
        if (--slot->unknown00 == 0) {
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
        /* Retail spins if the typed lookup finds no entry. */
        for (;;) {
        }
    }
}
