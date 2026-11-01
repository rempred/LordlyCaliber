#include "game/effect_model_e06_internal.h"

void func_00268400(s32 count, E06TransformOutput *input, E06TripleNode **nodes)
{
    s32 i;
    for (i = 0; i < count; i++) {
        if (*nodes != 0) {
            (*nodes)->unk04 = (s32)(input[i].m[3][0] * 30.0f);
            (*nodes)->unk08 = (s32)(input[i].m[3][1] * 30.0f);
            (*nodes)->unk0C = (s32)(input[i].m[3][2] * 30.0f);
        }
        nodes++;
    }
}
