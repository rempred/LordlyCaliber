#include "game/chapter_card.h"

s32 func_0028ac88(s32 format, u32 width)
{
    u32 rowBytes;

    if (format == 3) {
        rowBytes = ((width + 3) >> 2) << 4;
    } else if (format == 2) {
        rowBytes = ((width + 3) >> 2) << 3;
    } else if (format != 1) {
        rowBytes = ((width + 15) >> 4) << 3;
    } else {
        rowBytes = ((width + 7) >> 3) << 3;
    }
    return rowBytes;
}
