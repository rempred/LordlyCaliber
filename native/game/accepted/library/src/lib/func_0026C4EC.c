#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;

extern int func_0020BFF8(void *);
extern int func_0020C2D8(void *);

void func_0026C4EC(void *object, void *outputView)
{
    E04Vec3 *output = (E04Vec3 *)outputView;
    int first = func_0020BFF8(object);

    if (first != func_0020C2D8(object)) {
        output->x = 1.0f;
    } else {
        output->x = -1.0f;
    }
    output->y = 0.0f;
    output->z = 0.0f;
}
