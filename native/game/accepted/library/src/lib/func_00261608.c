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

void *func_00261608(u16 code, s32 key04)
{
    void *data;
    /* Keep this cached result separate from data. KMC otherwise folds the
     * return paths together or moves the normal return past the default spin. */
    void *cachedResult;
    ResourceSlot *slot;
    u16 slotCode;

    switch (code) {
    case 4:
        data = 0;
        if (key04 > 0) {
            data = func_00260B0C(4, key04, 0);
            if (data != 0) {
                goto cached;
            }
            slot = func_00260B98();
            if (slot != 0) {
                data = func_00260F30(4, key04);
                if (data != 0) {
                    slot->unknown00 = 1;
                    slotCode = 4;
                    goto register_slot;
                } else {
                    /* This code retains its separate allocation-failure spin. */
                    for (;;) {
                    }
                }
            }
        }
        goto done;
    case 7:
        data = 0;
        if (key04 > 0) {
            data = func_00260B0C(7, key04, 0);
            if (data != 0) {
                goto cached;
            }
            slot = func_00260B98();
            if (slot != 0) {
                data = func_00260F30(7, key04);
                if (data != 0) {
                    slot->unknown00 = 1;
                    slotCode = 7;
                    goto register_slot;
                }
            }
        }
        goto done;
    case 8:
        data = 0;
        if (key04 > 0) {
            data = func_00260B0C(8, key04, 0);
            if (data != 0) {
cached:
                func_00260B44(data);
                cachedResult = data;
                goto cached_done;
            }
            slot = func_00260B98();
            if (slot != 0) {
                data = func_00260F30(8, key04);
                if (data != 0) {
                    slot->unknown00 = 1;
                    slotCode = 8;
register_slot:
                    slot->data0C = data;
                    slot->code02 = slotCode;
                    slot->key04 = key04;
                    slot->key08 = 0;
                }
            }
        }
done:
        return data;
    default:
        /* Retail spins for an unsupported code. */
        for (;;) {
        }
    }
cached_done:
    return cachedResult;
}
