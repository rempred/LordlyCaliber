#include "game/effect_math_e04.h"

extern float D_801D06F0;
extern float D_801D0740;
extern float D_801D0764;
/* Only the halfword store widths are established for these two globals. */
extern unsigned short D_801D0820;
extern unsigned short D_801D071C;

void func_0026CCB4(float first, float second, float third, int value)
{
    unsigned int inverse = 255u - (unsigned int)value;

    D_801D06F0 = first;
    D_801D0740 = second;
    D_801D0764 = third;
    D_801D0820 = value;
    D_801D071C = inverse;
}
