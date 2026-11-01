#include "game/effect_dispatch_e07.h"

typedef struct Func0026A510Record {
    u8 unknown_00[0x30];
    float value_30;
    float value_34;
    float value_38;
    u8 unknown_3C[8];
} Func0026A510Record;

typedef struct Func0026A510Output {
    u32 unknown_00;
    s32 value_04;
    s32 value_08;
    s32 value_0C;
    u8 unknown_10[8];
    u16 flags_18;
    u16 unknown_1A;
} Func0026A510Output;

extern void func_0025F058(void *source, void *result);
extern void mtx_apply_vec3(float matrix[4][4], float x, float y, float z,
                           float *out_x, float *out_y, float *out_z);
extern float D_80220EE0[4][4];

void func_0026a510(float first, float second, float third, s32 count,
                   Func0026A510Record *records, Func0026A510Output **slots)
{
    float matrix[4][4];
    s32 item;

    func_0025F058(D_80220EE0, matrix);
    matrix[3][0] = first;
    matrix[3][1] = second;
    matrix[3][2] = third;
    for (item = 0; item < count; item++) {
        if (slots[item]) {
            mtx_apply_vec3(matrix, records[item].value_30, records[item].value_34,
                          records[item].value_38, &first, &second, &third);
            slots[item]->value_04 = (s32)(first * 30.0f);
            slots[item]->value_08 = (s32)(second * 30.0f);
            slots[item]->value_0C = (s32)(third * 30.0f);
        }
    }
}