#include "game/effect_adapters_e08.h"

u32 func_0024BCA0(u32 argument)
{
    /* The public word carrier is consumed through its low byte. */
    u8 value = (u8)argument;

    if (value < 20) {
        return 0;
    }
    if ((u8)(value - 20) < 27) {
        return 1;
    }
    return (u8)(value - 47) < 40 ? 2 : 255;
}
