#include "game/effect_math_e04.h"

typedef struct {
    float x;
    float y;
    float z;
} E04Vec3;

extern float sqrtf(float);

void func_0025EFC8(void *vector)
{
    float inverse;

    /* Use the parameter directly: a local pointer copy changes KMC's save
     * scheduling around the possible sqrtf call. */
    inverse = 1.0f / __builtin_sqrtf(((E04Vec3 *)vector)->x * ((E04Vec3 *)vector)->x
                                  + ((E04Vec3 *)vector)->y * ((E04Vec3 *)vector)->y
                                  + ((E04Vec3 *)vector)->z * ((E04Vec3 *)vector)->z);
    ((E04Vec3 *)vector)->x *= inverse;
    ((E04Vec3 *)vector)->y *= inverse;
    ((E04Vec3 *)vector)->z *= inverse;
}
