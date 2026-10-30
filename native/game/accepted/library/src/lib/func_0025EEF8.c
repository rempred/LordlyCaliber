#include "game/effect_math_e04.h"

void func_0025EEF8(void *leftView, void *rightView, void *resultView)
{
    float *left = (float *)leftView;
    float *right = (float *)rightView;
    float *result = (float *)resultView;
    float temporary[3];

    temporary[0] = left[1] * right[2] - left[2] * right[1];
    temporary[1] = left[2] * right[0] - left[0] * right[2];
    temporary[2] = left[0] * right[1] - left[1] * right[0];
    result[0] = temporary[0];
    result[1] = temporary[1];
    result[2] = temporary[2];
}
