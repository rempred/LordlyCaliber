#include "game/effect_model_e06_internal.h"

static inline void e06_initialize_nodes(E06Model *input, E06TransformOutput *matrices, E06Node **nodes, u16 code, s32 offset)
{
    u32 i;
    E06Node *node;
    E06NodePayload *payload;
    E06Record *record;
    for (i = 0; i < input->count; i++) {
        if (nodes[i] != 0) {
            nodes[i]->unk00 = func_00263CE4;
            node = nodes[i];
            payload = &node->payload;
            func_80093380(payload, sizeof(*payload));
            record = &input->records[i];
            if (record->unk06 != 0) {
                payload->unk04 = &matrices[i];
                node->payload.unk14 = func_00260B0C(6, record->unk06, 0);
                if (node->payload.unk14 == 0) for (;;) {}
                if (record->unk00 != 0) {
                    payload->unk08 = func_00260B0C(5, record->unk00, 0);
                    if (payload->unk08 == 0) for (;;) {}
                }
                payload->unk10 = record->unk04;
                if (!(record->unk02 & 0x8000)) {
                    payload->unk0C = func_00260B0C(code, record->unk02 + offset, 0);
                    if (payload->unk0C == 0) for (;;) {}
                }
            }
        }
    }
}
struct ResourceBlock;
static inline void e06_create_nodes(E06Model *input, void **output)
{
    u32 i;
    for (i = 0; i < input->count; i++) {
        if (input->records[i].unk06 != 0) output[i] = func_00261D24();
        else output[i] = 0;
    }
}

void func_00268890(E06State *state, s32 key, u16 code, s32 offset, u16 unk10, s32 unk14)
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
    state->unk10 = func_00261A44((struct ResourceBlock *)model, code, offset, unk10, unk14);
    e06_initialize_nodes(model, state->matrices, (E06Node **)state->nodes, code, offset);
}
