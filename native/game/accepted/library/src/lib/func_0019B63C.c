#include "common/types.h"

typedef struct {
    u16 item;
    u8 field02;
    u8 field03;
} ShopEntry;

extern ShopEntry D_80193AC0[];
extern u16 func_8016B738(u16 item);

void func_0019B63C(u16 item, u32 amount)
{
    /* This separate value preserves the pinned compiler's saved lifetime
     * across the lookup for the new-slot update. */
    u32 new_slot_amount = amount;
    u16 slot;
    s32 index;
    if (item == 0) {
        return;
    }
    slot = func_8016B738(item);
    if (slot == 0x1FF) {
        for (index = 0; index < 40; index++) {
            if (D_80193AC0[index].item == 0) {
                D_80193AC0[index].item = item;
                D_80193AC0[index].field02 += new_slot_amount;
                break;
            }
        }
    } else {
        D_80193AC0[slot].field02 += amount;
    }
}
