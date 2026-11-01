#include "game/effect_model_e06_internal.h"

void func_00268358(s32 count, void **objects)
{
    s32 i;
    for (i = 0; i < count; i++) {
        if (objects[i] != 0) func_001F0F6C((s32)objects[i]);
    }
}
