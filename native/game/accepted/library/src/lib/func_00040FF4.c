#include "game/scenario_item_views.h"

extern u16 D_801953F2;
extern u16 D_80190EBC[];
extern u16 D_8019532C[];
u8 func_00040FF4(u8 alternate, u8 target)
{
    u16 step;
    s16 remaining;
    s16 count;
    if (alternate) {
        step = D_801953F2 / 3U;
        remaining = D_8019532C[target];
    } else {
        step = D_80190F82.entries[0] / 3U;
        remaining = D_80190EBC[target];
    }
    count = 0;
    while (remaining > 0) { remaining -= step; count++; }
    if (count >= 4) count = 3;
    return count;
}
