#include "game/effect_runtime_e03_internal.h"

void *func_00262694(void)
{
    E03Task *task;

    if (D_80220E8E != 0xFFFF) {
        task = &D_80220D84[D_80220E8E];
        D_80220E8E = task->next_free_00;
    } else {
        for (;;) {
        }
    }
    return task;
}
