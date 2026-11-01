/* Extended form of accepted func_000bbd50: retain byte-sized class IDs and
 * return zero outside the active roster, reading the editable family resource.
 * The packed word contains behavior:family as u16:u16. Existing callers also
 * require behavior in v1. The empty register constraint preserves that custom
 * ABI without hand-written instructions; an ordinary u64 return exceeds the
 * original 48-byte allocation with the matching compiler.
 */
#include "class_menu_config.h"
typedef unsigned int u32;

u32 lc_family_lookup(int class_id)
{
    u32 index = class_id & 255;
    register u32 packed __asm__("$2");
    register u32 behavior __asm__("$3");
    if (index >= LC_CLASS_COUNT) {
        packed = 0;
    } else {
        unsigned char *owner = *(unsigned char **)LC_OWNER_POINTER;
        unsigned char *resource = *(unsigned char **)(owner + 0x80);
        packed = *(u32 *)(resource + LC_FAMILY_OFFSET + index * 4);
    }
    behavior = packed >> 16;
    __asm__ __volatile__("" : : "r" (behavior));
    return packed;
}
