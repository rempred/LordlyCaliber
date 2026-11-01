#include "game/effect_model_e06_internal.h"

void func_002682A4(float root[4][4], E06HierarchyInput *input, E06TransformOutput *output)
{
    /* Indexed matrix-chain scratch; the frame alone does not prove the bound. */
    float (*chain[32])[4];
    float (*matrix)[4];
    u32 i;
    u32 level;
    chain[0] = root;
    for (i = 0; i < input->count; i++) {
        matrix = output[input->pairs[i].index].m;
        level = input->pairs[i].level;
        guMtxCatF(matrix, chain[level], matrix);
        chain[level + 1] = matrix;
    }
}
