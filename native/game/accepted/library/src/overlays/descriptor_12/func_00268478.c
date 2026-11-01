#include "game/effect_model_e06_internal.h"

void func_00268478(s32 count, E06TransformOutput *output, float value)
{
    s32 i;
    for (i = 0; i < count; i++, output++) output->unk40 = value;
}
