#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;

extern void func_00028C40(float [4][4]);

void func_0026C230(void *vectorView, void *matrixView)
{
    E04Vec3 *vector = (E04Vec3 *)vectorView;
    float (*matrix)[4] = (float (*)[4])matrixView;
    float basis[3][3];
    int row;
    int column;
    int i;

    basis[2][0] = vector->x;
    basis[2][1] = vector->y;
    basis[2][2] = vector->z;
    func_0025EFC8((E04Vec3 *)basis[2]);
    basis[1][0] = 0.0f;
    basis[1][1] = 1.0f;
    basis[1][2] = 0.0f;
    func_0025EEF8(basis[1], basis[2], basis[0]);
    func_0025EEF8(basis[2], basis[0], basis[1]);
    func_00028C40(matrix);
    for (row = 0, i = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            matrix[row][column] = ((float *)basis)[i++];
        }
    }
}
