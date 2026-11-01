#include "game/effect_model_e06_internal.h"

static inline void e06_clear_nodes(E06State *state)
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
struct ResourceBlock;
void func_00268ED4(E06State *state)
{
    func_002619A4((struct ResourceBlock *)state->model);
    e06_clear_nodes(state);
    func_800712C4(state->allocation);
    func_00260C24(state->model);
}
