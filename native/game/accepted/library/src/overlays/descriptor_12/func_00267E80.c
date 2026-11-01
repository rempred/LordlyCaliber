#include "game/effect_model_e06_internal.h"

struct Func00269F00Curve;
extern float func_00267990(struct Func00269F00Curve *curve, float value);
static __inline void e06_apply_values(float *values, E06TransformOutput *output)
{
    func_00022e90(output->m, values[3], values[4], values[5]);
    output->m[0][0] *= values[0];
    output->m[0][1] *= values[0];
    output->m[0][2] *= values[0];
    output->m[1][0] *= values[1];
    output->m[1][1] *= values[1];
    output->m[1][2] *= values[1];
    output->m[2][0] *= values[2];
    output->m[2][1] *= values[2];
    output->m[2][2] *= values[2];
    output->m[3][0] = values[6];
    output->m[3][1] = values[7];
    output->m[3][2] = values[8];
    output->unk40 = values[9];
}

void func_00267E80(float value, struct Func00269F00Curve **curves,
                   E06TransformOutput *output, float *values)
{
    s32 i;
    for (i = 0; i < 10; i++) {
        if (curves[i] != 0) {
            values[i] = func_00267990(curves[i], value);
        }
    }
    e06_apply_values(values, output);
}
