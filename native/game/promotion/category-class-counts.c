/* Category class list and current/eligible roster counts (Army BBDAC).
 * This is modified-game C, not an accepted byte-matching decompilation.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
extern u8 *volatile lc_army_owner;
extern u8 lc_roster[], lc_named_class_map[], lc_class_records[];
void lc_clear(void *data, u32 size);
u8 lc_check_change(u8 actor, u8 target);

#if LC_FAMILY_TABS
#define roster_class lc_family_roster_class
u8 roster_class(const u8 *actor);
#else
static u8 roster_class(const u8 *actor);
#endif

#ifndef LC_FAMILY_ROSTER_ONLY

u16 lc_category_class_counts(u16 category)
{
    u32 categoryBit = 1U << ((category - 1U) & 31), count = 0;
    u32 id, actorId, index;
    u8 *owner = lc_army_owner;
    const u8 *actor, *masks;
#if LC_FAMILY_TABS
    const u8 *tabs = *(u8 **)(owner + 4) + 23496 + 8;
#endif
    const u8 *discovered = (const u8 *)LC_DISCOVERY_ADDRESS;
    const u8 *bits = (const u8 *)LC_BIT_MASK_ADDRESS;
    u16 *entries = (u16 *)(owner + LC_CATEGORY_CLASSES_OFFSET);
    u16 *current = (u16 *)(owner + LC_CATEGORY_CURRENT_OFFSET);
    u16 *eligible = (u16 *)(owner + LC_CATEGORY_ELIGIBLE_OFFSET);
#if LC_FAMILY_TABS
    if (category > 8) categoryBit = 0;
    lc_clear(current, LC_CATEGORY_ELIGIBLE_OFFSET - LC_CATEGORY_CURRENT_OFFSET + LC_CATEGORY_CAPACITY * 2);
#else
    lc_clear(current, LC_CATEGORY_CAPACITY * 2);
    lc_clear(eligible, LC_CATEGORY_CAPACITY * 2);
#endif
#if LC_MASK_RESOURCE_OFFSET >= 0
    masks = *(u8 **)(owner + 0x80) + LC_MASK_RESOURCE_OFFSET;
#else
    masks = (const u8 *)LC_MASK_ADDRESS;
#endif
    for (id = 1; id < LC_CLASS_COUNT; ++id) {
#if LC_MASK_RESOURCE_OFFSET >= 0
        u32 mask = masks[id];
#else
        u32 mask = masks[id * LC_MASK_STRIDE];
#endif
        if (!(discovered[id >> 3] & bits[id & 7]) ||
#if LC_FAMILY_TABS
            (tabs[id] ? tabs[id] != category : !(mask & categoryBit)))
#else
            !(mask & categoryBit))
#endif
            continue;
        entries[count] = id;
        ++count;
    }
    actor = lc_roster;
    for (actorId = 1; actorId < 100; ++actorId, actor += 56) {
        u32 mask;
        if (!actor[17]) continue;
        id = roster_class(actor);
#if LC_MASK_RESOURCE_OFFSET >= 0
        mask = id < LC_CLASS_COUNT ? masks[id] : 0;
#else
        mask = id < LC_CLASS_COUNT ? masks[id * LC_MASK_STRIDE] : 0;
#endif
#if LC_FAMILY_TABS
        if (tabs[id] ? tabs[id] != category : !(mask & categoryBit)) continue;
#else
        if (!(mask & categoryBit)) continue;
#endif
        for (index = 0; index < count && entries[index] != id; ++index) {}
        if (index == count) ++count;
        entries[index] = id;
        ++current[index];
        ++eligible[index];
    }
    entries[count] = 0;
    actor = lc_roster;
    for (actorId = 1; actorId < 100; ++actorId, actor += 56) {
        if (!actor[17]) continue;
        id = roster_class(actor);
        for (index = 0; index < count; ++index)
            if (id != (u8)entries[index] && lc_check_change(actorId, entries[index]))
                ++eligible[index];
    }
    return count;
}

#endif

#if !LC_FAMILY_TABS || defined(LC_FAMILY_ROSTER_ONLY)
#if !LC_FAMILY_TABS
static
#endif
u8 roster_class(const u8 *actor)
{
    u32 primary = actor[17];
    u8 secondary = actor[18], alias = lc_named_class_map[primary];
    if (alias && lc_class_records[primary * 72 + 69] == secondary)
        secondary = alias;
    return secondary;
}
#endif
