#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;


void func_0026C1F0(void *firstView, void *secondView, void *matrixView)
{
    E04Vec3 *first = (E04Vec3 *)firstView;
    E04Vec3 *second = (E04Vec3 *)secondView;
    float (*matrix)[4] = (float (*)[4])matrixView;
    E04Vec3 temporary;

    temporary.x = -first->x;
    temporary.y = -first->y;
    temporary.z = -first->z;
    func_0025F530(&temporary, second, matrix);
}
