#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;

extern float sqrtf(float);
extern void func_00028C40(float matrix[4][4]);

void func_0025F178(void *thirdView, void *firstView, void *secondView, void *matrixView)
{
    E04Vec3 *third = (E04Vec3 *)thirdView;
    E04Vec3 *first = (E04Vec3 *)firstView;
    E04Vec3 *second = (E04Vec3 *)secondView;
    float (*matrix)[4] = (float (*)[4])matrixView;
    float first_inverse;
    float second_inverse;
    float third_inverse;

    first_inverse = 1.0f / __builtin_sqrtf(first->x * first->x
                                  + first->y * first->y
                                  + first->z * first->z);
    first->x *= first_inverse;
    first->y *= first_inverse;
    first->z *= first_inverse;

    second_inverse = 1.0f / __builtin_sqrtf(second->x * second->x
                                  + second->y * second->y
                                  + second->z * second->z);
    second->x *= second_inverse;
    second->y *= second_inverse;
    second->z *= second_inverse;

    third_inverse = 1.0f / __builtin_sqrtf(third->x * third->x
                                  + third->y * third->y
                                  + third->z * third->z);
    third->x *= third_inverse;
    third->y *= third_inverse;
    third->z *= third_inverse;

    func_00028C40(matrix);
    matrix[0][0] = first->x;
    matrix[0][1] = first->y;
    matrix[0][2] = first->z;
    matrix[1][0] = second->x;
    matrix[1][1] = second->y;
    matrix[1][2] = second->z;
    matrix[2][0] = third->x;
    matrix[2][1] = third->y;
    matrix[2][2] = third->z;
}
