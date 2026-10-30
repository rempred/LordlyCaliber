#include "game/effect_math_e04.h"

extern float sqrtf(float);

float func_0025EF78(void *vectorView)
{
    float *vector = (float *)vectorView;
    return __builtin_sqrtf(vector[0] * vector[0]
                         + vector[1] * vector[1]
                         + vector[2] * vector[2]);
}
