#include "common/types.h"
#include "game/effect_presentation_p05.h"
#include "game/effect_presentation_p04.h"

extern float sqrtf(float);
extern float func_0002d300(float);

/* Descriptor 11 maps these pairs to ROM 0x25EDD0 and 0x25EDE0. */
extern const double D_8020A210;
extern const double D_8020A218;
extern const double D_8020A220;
extern const double D_8020A228;

static __inline__ void p05_vector_difference(float *difference,
                                            float *point,
                                            float *position)
{
    difference[0] = point[0] - position[0];
    difference[1] = point[1] - position[1];
    difference[2] = point[2] - position[2];
}

static __inline__ float p05_vector_angle(float *vector,
                                       const double *scale,
                                       const double *divisor)
{
    float angle;
    float result;

    /* Keep the native inline square root and fallback path. */
    angle = (double)func_0002d300(vector[2] /
                __builtin_sqrtf(vector[0] * vector[0] + vector[2] * vector[2]))
                * *scale / *divisor;
    if (vector[0] < 0.0f) {
        result = 360.0f - angle;
    } else {
        result = angle;
    }
    return result;
}

s32 func_0024C964(float *position, u8 index, float heading)
{
    float difference[3];
    float point[3];
    float side;
    float first_angle;
    float second_angle;

    func_0024BF04(index, point, 0);
    side = (0.0f < position[0]) ? 1.0f : -1.0f;
    point[2] -= side * 10.0f;
    p05_vector_difference(difference, point, position);
    first_angle = p05_vector_angle(difference, &D_8020A210, &D_8020A218);

    point[2] += side * 20.0f;
    p05_vector_difference(difference, point, position);
    second_angle = p05_vector_angle(difference, &D_8020A220, &D_8020A228);

    if ((first_angle <= heading) & (heading <= second_angle)) {
        return 1;
    }
    if (second_angle < heading) {
        return 2;
    }
    return 0;
}
