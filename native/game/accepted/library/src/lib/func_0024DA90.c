#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"





P05BuilderObject *func_0024DA90(u8 index, P05BuilderSpec *spec, float scale)
{
    P05Vec3 vector;

    func_0024BF04(index, &vector, spec->attributes_02 & 0xF0);
    return func_0024D6B8(&vector, spec, scale);
}
