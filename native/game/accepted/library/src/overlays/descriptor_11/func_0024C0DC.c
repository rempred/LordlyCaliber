#include "game/effect_adapters_e08.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

typedef s32 (*E08ScalarCallback)(u8 value);
extern E08ScalarCallback g_func_0024BCE8_callbacks[];
extern E08ScalarCallback g_func_0024BF04_callbacks_08[];
extern float func_001FF7EC(s16 first, s16 second);

void func_0024C0DC(u8 value, P01Vec3 *output,
                   float xOffset, float yOffset, float zOffset)
{
    s32 index;
    float third;

    if (value < 20) {
        index = 0;
    } else {
        index = 1;
        if ((u8)(value - 20) >= 27) {
            /* Keep the flag and halfword fallback through KMC's selector pass. */
            s32 valid = (u8)(value - 47) < 40;

            if (valid == 0) {
                index = (u16)(valid | 255);
            } else {
                index = 2;
            }
        }
    }

    output->x = (float)g_func_0024BCE8_callbacks[index](value) + xOffset;
    third = (float)g_func_0024BF04_callbacks_08[index](value) + zOffset;
    output->z = third;
    output->y = func_001FF7EC((s16)(s32)output->x, (s16)(s32)third) + yOffset;
}
