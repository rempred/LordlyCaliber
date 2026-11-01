#include "game/effect_model_e06_internal.h"

void func_00267828(E06ResourcePair *pairs)
{
    s32 i;
    for (i = 0; pairs[i].unk00 != -1 || pairs[i].unk02 != 0; i++) {
        func_00261560(pairs[i].unk02);
    }
}
