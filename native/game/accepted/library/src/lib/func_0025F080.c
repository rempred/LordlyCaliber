#include "game/effect_math_e04.h"

void func_0025F080(void *scaleView, void *matrixView, void *resultView)
{
    float *scale = (float *)scaleView;
    float (*matrix)[4] = (float (*)[4])matrixView;
    float *result = (float *)resultView;
    float temporary[4][4];
    int row;
    int column;
    int i;

    for (row = 0; row < 3; row++) {
        for (column = 0; column < 4; column++) {
            if (column != 3) {
                temporary[row][column] = matrix[row][column] * scale[row];
            } else {
                temporary[row][column] = matrix[row][column];
            }
        }
    }
    for (column = 0; column < 4; column++) {
        temporary[3][column] = matrix[3][column];
    }
    for (i = 0; i < 16; i++) {
        result[i] = ((float *)temporary)[i];
    }
}
