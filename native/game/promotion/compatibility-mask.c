/* Adapted from accepted pure-C func_000bbd80.c, OB64 Decomp 96b75a67.
 * The retail configuration must compile to the original complete owner.
 * Expanded promotion resources keep one mask per class after the family words.
 */
#include "class_menu_config.h"
typedef signed int s32;
typedef unsigned int u32;
typedef unsigned char u8;

u8 lc_class_compatibility(s32 arg0)
{
    u32 index;
    u8 result;

    index = arg0 & 0xFF;
    if (index >= LC_CLASS_COUNT) {
        result = 0;
    } else {
#if LC_MASK_RESOURCE_OFFSET >= 0
        u8 *owner = *(u8 **)LC_OWNER_POINTER;
        u8 *resource = *(u8 **)(owner + 0x80);
        result = resource[LC_MASK_RESOURCE_OFFSET + index];
#else
        result = *(u8 *)(LC_MASK_ADDRESS + (index * LC_MASK_STRIDE));
#endif
    }
    return result;
}
