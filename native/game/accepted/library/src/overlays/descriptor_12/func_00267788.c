#include "game/effect_model_e06_internal.h"

void func_00267788(s32 count, E06FloatNode **nodes, s32 increment)
{
    s32 i;
    for (i = 0; i < count; i++, nodes++) {
        if (*nodes != 0) {
            (*nodes)->unk08 += (float)increment;
        }
    }
}
