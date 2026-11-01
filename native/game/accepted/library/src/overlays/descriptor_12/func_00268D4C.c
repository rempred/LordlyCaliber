#include "game/effect_model_e06_internal.h"

struct ResourceBlock;
static inline void e06_create_nodes(E06Model *input, void **output)
{
    u32 i;
    for (i = 0; i < input->count; i++) {
        if (input->records[i].unk06 != 0) output[i] = func_00263074();
        else output[i] = 0;
    }
}

void func_00268D4C(E06State *state, s32 key)
{
    E06Model *model;
    s32 bytes;
    state->unk00 = key;
    model = func_002611F4(key);
    state->model = model;
    bytes = model->count * sizeof(E06TransformOutput);
    bytes += model->count * sizeof(void *);
    if (bytes & 0x3F) bytes = (bytes & ~0x3F) + 0x40;
    state->allocation = func_80070F30(bytes);
    func_80093380(state->allocation, bytes);
    state->matrices = state->allocation;
    state->nodes = (void **)((u8 *)state->allocation + model->count * sizeof(E06TransformOutput));
    e06_create_nodes(model, state->nodes);
    state->unk10 = func_002617B0((struct ResourceBlock *)model);
    func_00268BFC((struct E06Model *)(model), (struct E06TransformOutput *)(state->matrices), (struct E06RenderNode **)(state->nodes));
}
