#include "game/effect_model_e06_internal.h"

void func_00268E6C(E06State *state)
{
    s32 count = state->model->count;
    void **nodes = state->nodes;
    s32 i;
    for (i = 0; i < count; i++) {
        if (nodes[i] != 0) {
            func_002630CC(nodes[i]);
            nodes[i] = 0;
        }
    }
}
