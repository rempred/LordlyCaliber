#include "game/effect_adapters_e08.h"

typedef s32 (*Func0024BCE8Callback)(u8 value);
extern Func0024BCE8Callback g_func_0024BCE8_callbacks[];

s16 func_0024BCE8(u8 value)
{
    s32 index;

    if (value < 20) {
        index = 0;
    } else if ((u8)(value - 20) < 27) {
        index = 1;
    } else {
        /* Direct constant selection changes KMC's branch lowering.
         * Keep the flag separate and narrow the fallback to a halfword. */
        s32 valid = (u8)(value - 47) < 40;

        if (valid) {
            index = 2;
        } else {
            index = (u16)(valid | 255);
        }
    }
    return g_func_0024BCE8_callbacks[index](value) >= 0;
}
