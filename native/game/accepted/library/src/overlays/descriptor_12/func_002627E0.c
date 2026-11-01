#include "game/effect_runtime_e03_internal.h"

void func_002627E0(E03Task *task)
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
}
