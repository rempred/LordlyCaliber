#include "game/chapter_card.h"

u32 func_0028acdc(s32 format, u32 width)
{
    u32 tileWidth;

    /* Keep the format 3 and 2 tests separate to preserve the shared tail. */
    if (format == 3) {
        tileWidth = ((width + 3) >> 2) << 2;
    } else if (format == 2) {
        tileWidth = ((width + 3) >> 2) << 2;
    } else if (format != 1) {
        tileWidth = ((width + 15) >> 4) << 4;
    } else {
        tileWidth = ((width + 7) >> 3) << 3;
    }
    return tileWidth;
}
