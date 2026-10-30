#include "game/effect_math_e04.h"

void func_0025F6C0(unsigned short *values, int count)
{
    int i;
    int j;
    unsigned short temporary;

    if (count < 2) {
        return;
    }
    for (i = 0; i < count - 1; i++) {
        for (j = i; j < count; j++) {
            if (values[i] > values[j]) {
                temporary = values[i];
                values[i] = values[j];
                values[j] = temporary;
            }
        }
    }
}
