#include "game/effect_model_e06_internal.h"

static inline s32 e06_select_pair(s32 value, E06ResourcePair *pairs)
{
    s32 i = 0;
    for (;;) {
        if (pairs[i].unk00 == -1 && pairs[i].unk02 == 0) {
            i--;
            if (i < 0) i = 0;
            break;
        }
        if (value == pairs[i].unk00) break;
        if (value < pairs[i].unk00) {
            i--;
            if (i < 0) i = 0;
            break;
        }
        i++;
    }
    return pairs[i].unk02;
}
void func_002691EC(s32 value, E06Model *input, E06RenderNode **nodes)
{
    u32 i;
    u16 code;
    E06RenderPayload *payload;
    if (value <= input->unk00) {
        for (i = 0; i < input->count; i++) {
            if (nodes[i] != 0) {
                code = input->records[i].unk02;
                payload = &nodes[i]->payload;
                if (code & 0x8000) {
                    payload->unk0C = func_002637DC(7, e06_select_pair(value, input->pairs[code & 0x7FFF]));
                }
            }
        }
    }
}
