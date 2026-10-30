#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;


void func_0025F350(void *firstView, void *secondView, void *matrixView)
{
    E04Vec3 *first = (E04Vec3 *)firstView;
    E04Vec3 *second = (E04Vec3 *)secondView;
    float (*matrix)[4] = (float (*)[4])matrixView;
    E04Vec3 third;
    E04Vec3 temporary;
    float old_x;

    temporary.x = first->y * second->z - first->z * second->y;
    temporary.y = first->z * second->x - first->x * second->z;
    temporary.z = first->x * second->y - first->y * second->x;
    third.y = temporary.y;
    third.z = temporary.z;
    old_x = temporary.x;

    temporary.x = third.y * first->z - third.z * first->y;
    third.x = old_x;
    temporary.y = third.z * first->x - third.x * first->z;
    temporary.z = third.x * first->y - third.y * first->x;
    second->x = temporary.x;
    second->y = temporary.y;
    second->z = temporary.z;

    func_0025F178(first, second, &third, matrix);
}
