#include "game/resource_arena.h"

s32 resource_find_arena_index(void *pointer)
{
    s32 index;
    for (index = 0; index < D_800AEDE0; ++index) {
        if ((u32)pointer >= (u32)D_800AEDB0[index].base &&
            (u32)pointer < (u32)D_800AEDB0[index].end) {
            break;
        }
    }
    return index;
}
