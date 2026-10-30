#include "game/effect_math_e04.h"

void func_0025F058(void *sourceView, void *resultView)
{
    float *source = (float *)sourceView;
    float *result = (float *)resultView;
    int i;

    for (i = 0; i < 16; i++) {
        result[i] = source[i];
    }
}
