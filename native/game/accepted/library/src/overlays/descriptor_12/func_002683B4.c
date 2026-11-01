#include "game/effect_model_e06_internal.h"

void func_002683B4(s32 count, E06TransformOutput *input, E06FloatNode **nodes)
{
    s32 i;
    for (i = 0; i < count; i++, nodes++) {
        if (*nodes != 0) (*nodes)->unk08 = input->m[3][2] * 30.0f;
        input++;
    }
}
