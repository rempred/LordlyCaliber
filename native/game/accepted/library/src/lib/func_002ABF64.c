#include "game/actor_queries.h"

s32 func_002ABF64(s32 slot)
{
    QueryActor *actor = D_8022A974->actors[slot];
    u8 inputIndex;

    if (actor == 0 || (inputIndex = actor->inputIndex) == 0xFF) {
        return 0;
    }

    if (func_0020BFF8(&D_801CE8BC->inputs[inputIndex]) == 0) {
        return 1;
    }
    return 2;
}
