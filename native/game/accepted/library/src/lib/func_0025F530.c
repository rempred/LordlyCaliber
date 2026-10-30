#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;


void func_0025F530(void *firstView, void *secondView, void *matrixView)
{
    E04Vec3 *first = (E04Vec3 *)firstView;
    E04Vec3 *second = (E04Vec3 *)secondView;
    float (*matrix)[4] = (float (*)[4])matrixView;
    E04Vec3 third;
    E04Vec3 temporary;
    float old_x;
    float y_product;

    temporary.x = second->y * first->z - second->z * first->y;
    temporary.y = second->z * first->x - second->x * first->z;
    temporary.z = second->x * first->y - second->y * first->x;
    third.y = temporary.y;
    third.z = temporary.z;
    old_x = temporary.x;

    temporary.x = first->y * third.z - first->z * third.y;
    y_product = first->z * old_x;
    third.x = old_x;
    temporary.y = y_product - first->x * third.z;
    temporary.z = first->x * third.y - first->y * third.x;
    second->x = temporary.x;
    second->y = temporary.y;
    second->z = temporary.z;

    func_0025F178(first, &third, second, matrix);
}
