/* Army class-change screen setup, adapted from the accepted HYBRID_C
 * func_000d0150 at Decomp 96b75a67. The modified implementation uses ordinary
 * C control flow and explicit class/list bindings. No byte-match claim.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
extern u8 *volatile lc_army_owner;
extern u8 lc_roster[], lc_named_class_map[], lc_class_records[];
void lc_prepare_class(u32 target, u8 secondary, u8 alias, u8 previous);
void lc_prepare_actor(u8 actor);
void lc_family_menu(void);
void lc_equipment_summary(void);
u16 lc_equipment_entry(u32 unused, u8 actor, u8 slot);
u32 lc_family_lookup(u8 id);

void lc_class_screen(u8 preserveSelection)
{
    u8 *owner = lc_army_owner;
    u32 actorId = owner[0x18B], index, count, behavior;
    const u8 *actor;
    u8 primary, secondary, alias, previous, target;
    if (actorId == 0 || actorId >= 100) {
        owner[0x5EC] = 0;
        *(u16 *)(owner + 0x5E8) = 0;
        *(u16 *)(owner + 0x5EA) = 0;
        *(u16 *)(owner + LC_LIST_OFFSET) = 0;
        *(u16 *)(owner + 0x1BFC) = 1;
        for (index = 0; index < 4; ++index)
            ((u16 *)(owner + 0x1BFE))[index] = 0;
        owner[0x60C] = 0;
        *(u16 *)(owner + 0x1BFA) = 0;
        *(u16 *)(owner + 0x1BF6) = 0;
        return;
    }
    previous = preserveSelection ? owner[0x5E9] : 0;
    actor = lc_roster + (actorId - 1) * 56;
    primary = actor[17];
    secondary = actor[18];
    alias = lc_named_class_map[primary];
    target = secondary;
    if (alias && lc_class_records[primary * 72 + 69] == secondary)
        target = alias;
    owner[0x5EC] = target;
    *(u16 *)(owner + 0x5E8) = target;
    if (preserveSelection) target = previous;
    lc_prepare_class(target, secondary, alias, previous);
    lc_prepare_actor(actorId);
    lc_family_menu();
    lc_equipment_summary();
    owner = lc_army_owner;
    count = *(u16 *)(owner + 0x1BFC);
    *(u16 *)(owner + 0x5EA) = 0;
    for (index = 0; index < count; ++index) {
        u32 entry = ((u16 *)(owner + LC_LIST_OFFSET))[index];
#if LC_DISABLED_FLAG == 128
        entry &= 127;
#else
        entry &= 255;
#endif
        if (entry == target) {
            *(u16 *)(owner + 0x5EA) = index;
            break;
        }
    }
    for (index = 0; index < 4; ++index) {
        u16 item = lc_equipment_entry(0, actorId, index + 1);
        ((u16 *)(lc_army_owner + 0x1BFE))[index] = item;
    }
    /* The source-built lookup packs inherited behavior in the high half. */
    behavior = lc_family_lookup(lc_army_owner[0x5E9]) >> 16;
    lc_army_owner[0x60C] = behavior == 1 || behavior == 2 || behavior == 5;
    *(u16 *)(lc_army_owner + 0x5E8) = target;
}
