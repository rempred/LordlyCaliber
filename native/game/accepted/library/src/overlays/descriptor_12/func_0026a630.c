#include "game/effect_dispatch_e07.h"

typedef struct Func0026A630Record {
    float matrix_00[4][4];
    u32 unknown_40;
} Func0026A630Record;

extern void func_0025F058(void *source, void *result);
extern void guMtxCatF(float left[4][4], float right[4][4], float result[4][4]);
extern float D_80220FF0[16];

void func_0026a630(float first, float second, float third, s32 count,
                   Func0026A630Record *records, void **slots)
{
    float matrix[4][4];
    s32 item;

    func_0025F058(D_80220FF0, matrix);
    matrix[3][0] = first;
    matrix[3][1] = second;
    matrix[3][2] = third;
    for (item = 0; item < count; item++) {
        if (slots[item]) {
            guMtxCatF(records[item].matrix_00, matrix, records[item].matrix_00);
        }
    }
}