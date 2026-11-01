#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_math_e04.h"


/* Observed prefix and matrix view; the remaining object layout is unresolved. */
typedef struct P05ScaledObject {
    u8 unknown_00[0xA];
    u16 flags_0A;
    u8 unknown_0C[0x10];
    float matrix_1C[4][4];
} P05ScaledObject;


P05ScaledObject *func_0024E430(u16 value, float scale)
{
    P05Vec3 scaling;
    P05ScaledObject *object = func_0026C070(value);

    object->flags_0A |= 0x8000;
    scaling.x = scaling.y = scaling.z = scale;
    func_0025F080(&scaling, object->matrix_1C, object->matrix_1C);
    return object;
}
