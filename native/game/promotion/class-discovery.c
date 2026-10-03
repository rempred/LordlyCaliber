/* Modified-game C for US Rev 0 z64 0xC3DAC..0xC3F50.
 * Reconstructed from the original owner, not an accepted matching-C claim.
 * Named-class aliases fall back to the character's secondary class. The two
 * original roster walks intentionally have different final actor indices.
 */
#include "class_menu_config.h"
typedef unsigned char u8;
extern u8 lc_roster[];
extern u8 lc_named_class_map[];
extern int lc_class_eligible(unsigned char actor, unsigned char target);

void lc_discover_classes(void)
{
    u8 *flags = (u8 *)LC_DISCOVERY_ADDRESS;
    u8 *actor = lc_roster;
    unsigned int index, target, primary;
    for (index = 1; index < 100; ++index, actor += 56) {
        primary = actor[17];
        target = primary < LC_RECORD_COUNT ? lc_named_class_map[primary] : 0;
        if (target == 0) target = actor[18];
        if (primary != 0 && target < LC_CLASS_COUNT)
            flags[target >> 3] |= 1U << (target & 7);
    }
    /* Keep the retail terminal-row calls. Expanded layouts supply their own
     * bounded scan count; it must never exceed the discovery allocation or
     * the one-byte target-ID range.
     */
    for (target = 0; target < LC_DISCOVERY_SCAN_COUNT; ++target) {
        if (flags[target >> 3] & (1U << (target & 7))) continue;
        for (index = 1; index < 99; ++index) {
            if ((u8)lc_class_eligible(index, target)) {
                flags[target >> 3] |= 1U << (target & 7);
            }
        }
    }
}
