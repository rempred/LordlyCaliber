#include "game/shop_price_interface.h"

extern u32 D_80196A6C;
/* Price halfwords in the two tables have 12-byte and 32-byte row strides. */
extern u16 D_8018E6D2[];
extern u16 D_8018C414[];
extern u8 D_80193AC3[];
extern u8 D_80196B03[];
extern u16 func_8016B738(u16 item_id);
extern u16 func_0016B6FC(u16 item_id);

u8 func_0019B26C(u16 item_id, u8 kind)
{
    u16 quantity;
    u16 slot;
    s32 remaining;

    if (kind == 0) {
        s32 price;
        s32 count;
        quantity = D_80196A6C / D_8018E6D2[item_id * 6];
        slot = func_8016B738(item_id);
        if (slot == 0x1FF) {
            if (quantity >= 100) {
                quantity = 99;
            }
        } else {
            remaining = 99 - D_80193AC3[slot * 4];
            count = quantity;
            if (count > remaining) {
                count = remaining;
            }
            quantity = count;
        }
        /* This path reloads the price after the slot lookup. */
        price = D_8018E6D2[item_id * 6];
        /* The value-producing clamp preserves the compiler's shared assignment
         * tail; an update-in-place if removes four instructions from this owner. */
        quantity = quantity * price > 99999 ? 99999 / price : quantity;
    } else {
        s32 price;
        s32 count;
        if (item_id == 0xFA) {
            price = func_0019BD14();
        } else {
            price = D_8018C414[item_id * 16];
        }
        quantity = D_80196A6C / (u32)price;
        slot = func_0016B6FC(item_id);
        if (slot == 0x1FF) {
            if (quantity >= 100) {
                quantity = 99;
            }
        } else {
            remaining = 99 - D_80196B03[slot * 4];
            count = quantity;
            if (count > remaining) {
                count = remaining;
            }
            quantity = count;
        }
        quantity = quantity * price > 99999 ? 99999 / price : quantity;
    }
    return quantity;
}
