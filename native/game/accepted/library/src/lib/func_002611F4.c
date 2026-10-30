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

extern void *func_00260B0C(u16 code, s32 key04, s32 key08);
extern void func_00260B44(void *data);
extern ResourceSlot *func_00260B98(void);
extern void *func_00260F30(u16 code, s32 index);
struct ResourceBlock;
extern void func_0026109C(struct ResourceBlock *block);

void *func_002611F4(s32 key04)
{
    void *data = 0;
    ResourceSlot *slot;

    if (key04 > 0) {
        data = func_00260B0C(3, key04, 0);
        if (data != 0) {
            func_00260B44(data);
            return data;
        }
        slot = func_00260B98();
        if (slot != 0) {
            data = func_00260F30(3, key04);
            if (data != 0) {
                func_0026109C(data);
                slot->unknown00 = 1;
                slot->data0C = data;
                slot->code02 = 3;
                slot->key04 = key04;
                slot->key08 = 0;
            } else {
                /* Retail spins if the resource allocation fails. */
                for (;;) {
                }
            }
        }
    }
    return data;
}
