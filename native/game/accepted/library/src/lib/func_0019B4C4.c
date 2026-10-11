#include "common/types.h"

typedef struct {
    u16 item;
    u8 field02;
    u8 field03;
} ShopEntry;

extern ShopEntry D_80193AC0[];
extern ShopEntry D_80196B00[];
extern u16 func_8016B738(u16 item);
extern u16 func_8016B6FC(u16 item);

void func_0019B4C4(u16 item, u32 amount, u8 mode)
{
    /* Retain the word amount separately for insertion, as in B63C. */
    u32 new_slot_amount = amount;
    u16 slot;
    s32 index;
    if (item == 0) {
        return;
    }
    if (mode == 0) {
        slot = func_8016B738(item);
        if (slot == 0x1FF) {
            for (index = 0; index < 40; index++) {
                if (D_80193AC0[index].item == 0) {
                    D_80193AC0[index].item = item;
                    D_80193AC0[index].field03 += new_slot_amount;
                    break;
                }
            }
        } else {
            D_80193AC0[slot].field03 += amount;
        }
    } else {
        slot = func_8016B6FC(item);
        if (slot == 0x1FF) {
            for (index = 0; index < 278; index++) {
                if (D_80196B00[index].item == 0) {
                    D_80196B00[index].item = item;
                    D_80196B00[index].field03 += new_slot_amount;
                    break;
                }
            }
        } else {
            D_80196B00[slot].field03 += amount;
        }
    }
}
