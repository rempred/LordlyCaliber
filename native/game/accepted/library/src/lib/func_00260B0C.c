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

void *func_00260B0C(u16 code, s32 key04, s32 key08)
{
    void *data = 0;
    ResourceSlot *slot = func_00260828(code, key04, key08);

    if (slot != 0) {
        data = slot->data0C;
    }
    return data;
}
