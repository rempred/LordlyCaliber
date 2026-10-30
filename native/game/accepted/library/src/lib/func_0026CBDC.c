#include "game/effect_math_e04.h"

float func_0026CBDC(float value, int count, float *a, float *b, float *c)
{
    int lower = 0;
    int upper = count - 1;
    int middle;
    float offset;
    float width;

    while (lower < upper) {
        middle = (lower + upper) / 2;
        if (a[middle] < value) {
            lower = middle + 1;
        } else {
            upper = middle;
        }
    }
    lower -= lower > 0;
    offset = value - a[lower];
    width = a[lower + 1] - a[lower];
    return (((c[lower + 1] - c[lower]) * offset / width
             + 3.0f * c[lower]) * offset
            + ((b[lower + 1] - b[lower]) / width
               - (2.0f * c[lower] + c[lower + 1]) * width)) * offset
           + b[lower];
}
