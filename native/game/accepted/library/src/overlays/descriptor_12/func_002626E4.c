#include "game/effect_runtime_e03_internal.h"

void func_002626E4(void *taskView)
{
    E03Task *task = taskView;
    task->callback_04 = 0;
    task->field_14 = 0;
    task->next_10 = 0;
    task->previous_0C = 0;
    task->state_08 = 0;
    task->next_free_00 = D_80220E8E;
    D_80220E8E = task - D_80220D84;
}
