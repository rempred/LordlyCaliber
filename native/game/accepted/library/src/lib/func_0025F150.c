#include "game/effect_math_e04.h"

void func_0025F150(void *matrixView, void *valuesView)
{
    float (*matrix)[4] = (float (*)[4])matrixView;
    float *values = (float *)valuesView;
    int i;

    for (i = 0; i < 3; i++) {
        matrix[3][i] = values[i];
    }
}
