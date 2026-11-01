#include "game/effect_runtime_e03_internal.h"

void *func_00262C04(void *parentView, void *callback)
{
    E03Task *parent = parentView;
    E03Task *task;
    E03Task *result;

    task = parent->next_10;
    result = 0;
    while (task != 0) {
        if (task->field_14 != parent) {
            break;
        }
        if (task->callback_04 == (E03TaskCallback)callback) {
            result = task;
            break;
        }
        task = task->next_10;
    }
    return result;
}
