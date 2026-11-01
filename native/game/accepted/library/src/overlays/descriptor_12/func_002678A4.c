#include "game/effect_model_e06_internal.h"

void func_002678A4(E06ResourcePair *pairs)
{
    s32 i;
    for (i = 0; pairs[i].unk00 != -1 || pairs[i].unk02 != 0; i++) {
        func_00260CE0(8, pairs[i].unk02, 0);
    }
}
