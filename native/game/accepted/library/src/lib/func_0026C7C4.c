#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;

extern int func_0025F770(void *);
extern int func_0025F7C0(void *);
extern int func_0025F798(void *);

void func_0026C7C4(void *object, void *outputView)
{
    E04Vec3 *output = (E04Vec3 *)outputView;
    output->x = (float)func_0025F770(object);
    output->z = (float)func_0025F7C0(object);
    output->y = (float)func_0025F798(object);
}
