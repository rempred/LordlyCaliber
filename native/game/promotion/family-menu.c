/* Modified-game class menu. Addresses and layout are supplied by the exporter.
 * Families use a u16 identity and a u16 inherited behavior, packed in one word.
 * This replaces the complete Army menu builder, not individual instructions.
 */
#include "class_menu_config.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 (*CheckChange)(unsigned int actor, unsigned int target);

#if LC_CLASS_COUNT < 1 || LC_CLASS_COUNT > 256
#error Class IDs must fit their byte representation
#endif
#if LC_LIST_CAPACITY < LC_CLASS_COUNT + 1
#error The class list needs room for every class and its terminator
#endif
#if LC_DISABLED_FLAG < LC_CLASS_COUNT
#error The disabled flag overlaps a class ID
#endif

void lc_build_class_menu(void)
{
    u8 *owner = *(u8 **)LC_OWNER_POINTER;
    const u32 *families = (const u32 *)(*(u8 **)(owner + 0x80) + LC_FAMILY_OFFSET);
    const u8 *discovered = (const u8 *)LC_DISCOVERY_ADDRESS;
    const u8 *bits = (const u8 *)LC_BIT_MASK_ADDRESS;
    u16 *entries = (u16 *)(owner + LC_LIST_OFFSET);
    u32 selected = *(u16 *)(owner + 0x5E8);
    u32 packed = selected < LC_CLASS_COUNT ? families[selected] : 0;
    u32 family = packed & 65535;
    u32 behavior = packed >> 16;
    u32 count = 0;
    u32 id;

    if (!family) {
        entries[count++] = selected | LC_DISABLED_FLAG;
    } else {
        for (id = 0; id < LC_CLASS_COUNT; ++id) {
            u32 entry;
            if ((families[id] & 65535) != family &&
                !(behavior == 1 && (id == 0x26 || id == 0x2B)) &&
                !(behavior == 2 && id == 0x26))
                continue;
            if (!(discovered[id >> 3] & bits[id & 7]))
                continue;
            entry = id;
            if (id == selected ||
                (behavior != 1 && behavior != 2 && behavior != 5) ||
                !((CheckChange)LC_CHECK_CHANGE)(owner[0x18B], id))
                entry |= LC_DISABLED_FLAG;
            entries[count++] = entry;
        }
    }
    *(u16 *)(owner + 0x1BFC) = count;
    entries[count] = 0;
}
