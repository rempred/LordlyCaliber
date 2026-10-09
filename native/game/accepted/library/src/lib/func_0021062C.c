#include "game/combat_types.h"

extern unsigned char *D_801D06D0;

int func_0021062C(int first, int second, int selector)
{
    switch ((u32)selector) {
    case 2: {
        int result = 0;
        if ((D_801D06D0[(u32)first - 1u] & 8) || (D_801D06D0[(u32)second - 1u] & 8)) result = 1;
        return result;
    }
    case 8: {
        int result = 0;
        if ((D_801D06D0[(u32)first - 1u] & 4) || (D_801D06D0[(u32)second - 1u] & 4)) result = 1;
        return result;
    }
    case 4: {
        int result = 0;
        if ((D_801D06D0[(u32)first - 1u] & 2) || (D_801D06D0[(u32)second - 1u] & 2)) result = 1;
        return result;
    }
    case 16: {
        int result = 0;
        if ((D_801D06D0[(u32)first - 1u] & 1) || (D_801D06D0[(u32)second - 1u] & 1)) result = 1;
        return result;
    }
    default:
        return 0;
    }
}
