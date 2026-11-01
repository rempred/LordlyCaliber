#include "game/effect_model_e06_internal.h"

void func_002677D0(s32 count, E06TripleNode **nodes, s32 increment)
{
    s32 i;
    E06TripleNode *node;
    for (i = 0; i < count; i++, nodes++) {
        node = *nodes;
        if (node != 0) {
            node->unk04 += increment;
            node->unk08 += increment;
            node->unk0C += increment;
        }
    }
}
