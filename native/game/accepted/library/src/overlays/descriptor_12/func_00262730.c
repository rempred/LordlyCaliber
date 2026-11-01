#include "game/effect_runtime_e03_internal.h"

void func_00262730(E03Task *after, E03Task *task)
{
    if (after == 0) {
        if (D_80220D8C != 0) {
            /* The for update retains KMC global-reload scheduling. */
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
}
