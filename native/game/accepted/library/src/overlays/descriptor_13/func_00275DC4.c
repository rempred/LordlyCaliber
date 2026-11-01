#include "game/effect_presentation_p06_internal.h"

typedef struct P06IntegerVector75DC4 {
    s32 value_00;
    s32 value_04, value_08, value_0C;
} P06IntegerVector75DC4;

void func_00275DC4(P06IntegerVector75DC4 *output, float *first, float *second)
{
    if (first[0] < second[0]) {
        second = first;
    }
    output->value_04 = second[0] * 30.0f + 9000.0f;
    output->value_08 = second[1] * 30.0f + 9000.0f;
    output->value_0C = second[2] * 30.0f + 9000.0f;
}
