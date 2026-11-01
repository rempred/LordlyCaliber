#include "game/effect_adapters_e08.h"
#include "game/effect_math_e04.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;


void func_0024E898(u8 value, s32 argument)
{
    P01Vec3 position;

    func_0024BF04(value, &position, 0x20);
    func_0026CCB4(position.x, position.y, position.z, argument);
}
