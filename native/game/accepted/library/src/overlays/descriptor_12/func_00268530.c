#include "game/effect_model_e06_internal.h"

void func_00268530(E06Model *input, E06TransformOutput *matrices, E06Node **nodes)
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
                if (record->unk02 != 0 && !(record->unk02 & 0x8000)) {
                    payload->unk0C = func_00260B0C(4, record->unk02, 0);
                    if (payload->unk0C == 0) for (;;) {}
                }
            }
        }
    }
}
