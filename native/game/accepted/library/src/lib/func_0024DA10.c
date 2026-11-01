#include "game/effect_adapters_e08.h"
#include "game/effect_presentation_p04.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;


void *func_0024DA10(u32 value, void *argument)
{
    P01Vec3 position;

    func_0024BF04((u8)value, &position, ((u8 *)argument)[2] & 0xF0);
    return func_0024D180(&position, argument);
}
