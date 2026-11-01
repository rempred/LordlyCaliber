#include "game/effect_adapters_e08.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

extern void func_00023780(void *destination, u32 bytes);


void func_0024C204(u8 *values, P01Vec3 *output, s32 flags)
{
    P01Vec3 sum;
    P01Vec3 current;
    s16 count;
    /* Keep the consumed byte flag view while retaining the word interface. */
    u8 flagBits = (u8)flags;

    func_00023780(&sum, sizeof(sum));
    for (count = 0; values[count] != 255; count++) {
        func_0024BF04(values[count], &current, flagBits);
        sum.y += current.y;
        sum.z += current.z;
    }

    /* Retail copies the last temporary x; only y and z are accumulated. */
    output->x = current.x;
    output->y = sum.y / (float)count;
    output->z = sum.z / (float)count;
}
