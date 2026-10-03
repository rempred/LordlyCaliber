/* Army category list, reconstructed from func_000BC0BC.
 * Mask 3 contributes the first two categories; every other nonzero mask
 * contributes only its lowest set bit. Preserve that rule for edited masks.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
extern u8 *volatile lc_army_owner;

u16 lc_class_categories(void)
{
    const u8 *discovered = (const u8 *)LC_DISCOVERY_ADDRESS;
    const u8 *bits = (const u8 *)LC_BIT_MASK_ADDRESS;
    const u8 *masks;
    u8 *owner;
    u32 id, seen = 0, count = 0;
#if LC_MASK_RESOURCE_OFFSET >= 0
    owner = lc_army_owner;
    masks = *(u8 **)(owner + 0x80) + LC_MASK_RESOURCE_OFFSET;
#else
    masks = (const u8 *)LC_MASK_ADDRESS;
#endif
    for (id = 1; id < LC_CLASS_COUNT; ++id) {
        u32 mask;
        if (!(discovered[id >> 3] & bits[id & 7]))
            continue;
#if LC_MASK_RESOURCE_OFFSET >= 0
        mask = masks[id];
#else
        mask = masks[id * LC_MASK_STRIDE];
#endif
        seen |= mask == 3 ? 3 : mask & (0U - mask);
    }
    owner = lc_army_owner;
    for (id = 0; id < 8; ++id)
        if (seen & (1U << id))
            owner[0x1E00 + count++] = id + 1;
    /* Original scans a ninth byte occupied by the saved KSEG0 return address.
     * Explicitly preserve the resulting category 9 without counting it.
     */
    owner[0x1E00 + count] = 9;
    return count;
}
