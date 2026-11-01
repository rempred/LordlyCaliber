#include "game/effect_presentation_p06_internal.h"

s32 func_00279920(u8 value)
{
    s16 kind = func_0024BCA0(value);
    if (kind == 0xFF)
        return -1;
    switch (kind) {
    case 0:
        return func_00279A50(func_0020C478(value));
    case 1:
        return func_0024BD60(value);
    case 2:
        return func_0024BD60(value);
    default:
        return -1;
    }
}
