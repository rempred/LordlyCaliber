#include "game/effect_model_e06_internal.h"

struct Func00269F00Curve;
extern float func_00267990(struct Func00269F00Curve *curve, float value);
static __inline void e06_apply_values(float *values, float matrix[4][4])
{
    func_00022e90(matrix, values[3], values[4], values[5]);
    matrix[0][0] *= values[0];
    matrix[0][1] *= values[0];
    matrix[0][2] *= values[0];
    matrix[1][0] *= values[1];
    matrix[1][1] *= values[1];
    matrix[1][2] *= values[1];
    matrix[2][0] *= values[2];
    matrix[2][1] *= values[2];
    matrix[2][2] *= values[2];
    matrix[3][0] = values[6];
    matrix[3][1] = values[7];
    matrix[3][2] = values[8];
}

static __inline void e06_curve_apply(float value, struct Func00269F00Curve **curves,
                   E06TransformOutput *output, float *values)
{
    s32 i;
    for (i = 0; i < 10; i++) {
        if (curves[i] != 0) {
            values[i] = func_00267990(curves[i], value);
        }
    }
    e06_apply_values(values, output->m);
    output->unk40 = values[9];
}

void func_00267FD0(s32 argument, E06TransformInput *input, E06TransformOutput *output)
{
    float values[10];
    float value = (float)argument / 30.0f;
    u32 item;
    u32 component;
    for (item = 0; item < input->count; item++) {
        for (component = 0; component < 9; component++) {
            values[component] = input->values[item][component];
        }
        values[9] = 1.0f;
        if (input->curves[item] != 0) {
            e06_curve_apply(value, input->curves[item], &output[item], values);
        } else {
            e06_apply_values(values, output[item].m);
            output[item].unk40 = 1.0f;
        }
    }
}
