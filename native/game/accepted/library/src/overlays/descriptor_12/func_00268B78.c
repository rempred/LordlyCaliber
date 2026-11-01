#include "game/effect_model_e06_internal.h"

void func_00268B78(E06Model *input, void **output)
{
    u32 i;
    for (i = 0; i < input->count; i++) {
        if (input->records[i].unk06 != 0) output[i] = func_00263074();
        else output[i] = 0;
    }
}
