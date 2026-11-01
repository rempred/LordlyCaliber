#include "game/effect_model_e06_internal.h"

void func_00268798(E06State *state)
{
    s32 count = state->model->count;
    void **nodes = state->nodes;
    s32 i;
    for (i = 0; i < count; i++) {
        if (nodes[i] != 0) {
            func_00261E30(nodes[i]);
            nodes[i] = 0;
        }
    }
}
