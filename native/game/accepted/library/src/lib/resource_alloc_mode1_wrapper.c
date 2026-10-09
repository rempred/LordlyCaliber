#include "game/resource_arena.h"

extern void *resource_alloc(u32 requested);

void *resource_alloc_mode1_wrapper(u32 requested)
{
    u16 saved_mode = D_800AEDE2;
    void *result;

    D_800AEDE2 = 1;
    result = resource_alloc(requested);
    D_800AEDE2 = saved_mode;
    return result;
}
