#include "game/effect_adapters_e08.h"
#include "common/types.h"
#include "game/effect_presentation_p04.h"

typedef struct P04Vec3 {
    float x, y, z;
} P04Vec3;

typedef struct P04PresentationSpec {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 value_04;
} P04PresentationSpec;

typedef struct P04PresentationObject P04PresentationObject;



P04PresentationObject *func_0024DF7C(u8 index, P04PresentationSpec *spec, void *argument)
{
    P04Vec3 vector;

    func_0024BF04(index, &vector, spec->value_02 & 0xF0);
    return func_0024DD7C(&vector, spec, argument);
}
