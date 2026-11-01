#include "game/effect_adapters_e08.h"

typedef struct P01Vec3 {
    float x;
    float y;
    float z;
} P01Vec3;

extern s32 func_0020D654(s32 first, s32 second);
extern void *func_0020C478(u32 index);
extern s32 func_0020C2C0(void *record);

s16 func_0024C2F8(P01Vec3 *position)
{
    float first = position->z / 38.0f;
    float second = position->x / 38.0f;
    s16 result;

    first += 1.0f;
    second += 4.0f;
    result = func_0020D654((s32)first, (s32)second);
    if (result != -1) {
        if (func_0020C2C0(func_0020C478(result)) != 0) {
            result = -1;
        }
    }
    return result;
}
