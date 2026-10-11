/* Standard-equipment purchase list, based on Army func_000BF5DC.
 * The class-line list flag is independent of family membership and the
 * disabled bit in each resulting row. Runtime addresses come from the export.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
extern u8 *volatile lc_army_owner;
extern volatile u32 lc_goth;
void lc_equipment_summary(void);
u32 lc_equipment_cost(u32 classId);
u8 lc_equipment_full(u32 classId);

void lc_equipment_class_menu(void)
{
    u8 *owner;
    const u8 *flags;
    const u8 *discovered = (const u8 *)LC_DISCOVERY_ADDRESS;
    const u8 *bits = (const u8 *)LC_BIT_MASK_ADDRESS;
    u16 *entries;
    u32 id, count = 0;
    lc_equipment_summary();
    owner = lc_army_owner;
    entries = (u16 *)(owner + LC_LIST_OFFSET);
#if LC_FLAGS_RESOURCE_OFFSET >= 0
    flags = *(u8 **)(owner + 0x80) + LC_FLAGS_RESOURCE_OFFSET;
#else
    flags = (const u8 *)LC_FLAGS_ADDRESS;
#endif
    for (id = 0; id < LC_CLASS_COUNT; ++id) {
        u32 cost;
#if LC_FLAGS_RESOURCE_OFFSET >= 0
        if (!(flags[id] & 0x80))
#else
        if (!(flags[id * LC_FLAGS_STRIDE] & 0x80))
#endif
            continue;
        if (!(discovered[id >> 3] & bits[id & 7]))
            continue;
        entries[count] = id;
        cost = lc_equipment_cost(id);
        if (lc_goth < cost || lc_equipment_full(id))
            entries[count] |= LC_DISABLED_FLAG;
        ++count;
    }
    entries[count] = 0;
    *(u16 *)(owner + 0x1BFC) = count;
}
