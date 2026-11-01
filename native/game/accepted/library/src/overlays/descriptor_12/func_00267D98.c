#include "game/effect_model_e06_internal.h"

void func_00267D98(float *values, float matrix[4][4])
{
    func_00022e90(matrix, values[3], values[4], values[5]);
    matrix[0][0] *= values[0];
    matrix[0][1] *= values[0];
    matrix[0][2] *= values[0];
    matrix[1][0] *= values[1];
    matrix[1][1] *= values[1];
    matrix[1][2] *= values[1];
    matrix[2][0] *= values[2];
    matrix[2][1] *= values[2];
    matrix[2][2] *= values[2];
    matrix[3][0] = values[6];
    matrix[3][1] = values[7];
    matrix[3][2] = values[8];
}
