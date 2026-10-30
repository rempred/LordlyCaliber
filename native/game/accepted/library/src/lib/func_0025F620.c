#include "game/effect_math_e04.h"

void func_0025F620(void *matrixView, void *vectorView, void *resultView)
{
    float (*matrix)[4] = (float (*)[4])matrixView;
    float *vector = (float *)vectorView;
    float *result = (float *)resultView;
    float temporary[4];
    int column;
    int row;

    for (column = 0; column < 4; column++) {
        temporary[column] = 0.0f;
        for (row = 0; row < 4; row++) {
            temporary[column] += matrix[row][column] * vector[row];
        }
    }
    for (row = 0; row < 4; row++) {
        result[row] = temporary[row];
    }
}
