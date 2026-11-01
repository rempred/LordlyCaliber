#include "game/effect_model_e06_internal.h"

void func_00267750(s32 count, E06FlagNode **nodes)
{
    s32 i;
    for (i = 0; i < count; i++, nodes++) {
        if (*nodes != 0) {
            (*nodes)->unk0C = 1;
        }
    }
}
