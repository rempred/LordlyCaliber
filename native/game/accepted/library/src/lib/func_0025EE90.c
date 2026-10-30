#include "game/effect_math_e04.h"

void func_0025EE90(void *sourceView, float scale, void *resultView)
{
    float *source = (float *)sourceView;
    float *result = (float *)resultView;
    float temporary[3];
    int i;

    for (i = 0; i < 3; i++) {
        temporary[i] = source[i] * scale;
    }
    for (i--; i >= 0; i--) {
        result[i] = temporary[i];
    }
}
