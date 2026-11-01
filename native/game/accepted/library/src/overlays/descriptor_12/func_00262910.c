#include "game/effect_runtime_e03_internal.h"

/* Keep acquisition inline to retain KMC returned-pointer lifetimes. */
static inline E03Task *e03AllocateTask(void)
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

void *func_00262910(void *callback, void *afterView)
{
    E03Task *after = afterView;
    E03Task *task;

    task = e03AllocateTask();
    if (task != 0) {
        task->state_08 = 0x100;
        task->callback_04 = 0;
        task->field_14 = 0;
        task->next_10 = 0;
        task->previous_0C = 0;
        if (after == 0) {
            if (D_80220D8C != 0) {
                /* Keep the same KMC traversal form as the list helper. */
                for (; D_80220D8C->next_10 != 0;
                     D_80220D8C = D_80220D8C->next_10) {
                }
                D_80220D8C->next_10 = task;
            }
            task->previous_0C = D_80220D8C;
            D_80220D8C = task;
            if (D_80220D88 == 0) {
                D_80220D88 = task;
            }
        } else {
            task->next_10 = after->next_10;
            after->next_10 = task;
            if (task->next_10 != 0) {
                task->next_10->previous_0C = task;
            }
            task->previous_0C = after;
            task->field_14 = after;
            if (task->next_10 == 0) {
                D_80220D8C = task;
            }
        }
        task->callback_04 = (E03TaskCallback)callback;
        D_80220D94[--D_80220D90] = task;
        D_80220FE8 = task;
        task->callback_04(0x100, task->payload_24);
        D_80220FE8 = D_80220D94[++D_80220D90];
    }
    return task;
}
