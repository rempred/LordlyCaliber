#include "game/effect_model_e06_internal.h"

void func_002684AC(E06Model *input, void **output)
{
    u32 i;
    for (i = 0; i < input->count; i++) {
        if (input->records[i].unk06 != 0) output[i] = func_00261D24();
        else output[i] = 0;
    }
}
