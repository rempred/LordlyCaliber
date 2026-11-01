#include "game/effect_runtime_e03_internal.h"

void func_00262AD4(E03Task *task)
{
    E03Task *next;
    E03Task *previous;

    next = task->next_10;
    previous = task->previous_0C;
    if (next != 0) {
        next->previous_0C = previous;
    }
    if (previous != 0) {
        previous->next_10 = next;
    }
    if (D_80220D88 == task) {
        D_80220D88 = next;
    }
    if (D_80220D8C == task) {
        D_80220D8C = previous;
    }
    task->callback_04 = 0;
    task->field_14 = 0;
    task->next_10 = 0;
    task->previous_0C = 0;
    task->state_08 = 0;
    task->next_free_00 = D_80220E8E;
    D_80220E8E = task - D_80220D84;
}
