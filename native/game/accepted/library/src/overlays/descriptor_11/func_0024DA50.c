#include "game/effect_adapters_e08.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;


void func_0024DA50(u8 value, void *argument)
{
    P01Vec3 position;

    func_0024BF04(value, &position, ((u8 *)argument)[2] & 0xF0);
    func_0024D41C(&position, argument);
}
