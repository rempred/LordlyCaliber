#include "game/effect_runtime_e03_internal.h"

void func_00262C94(void *recordView, s32 value, s32 resume_state)
{
    E03Pool94 *record = recordView;
    E03Task *task;

    task = D_80220FE8;
    task->resume_state_18 = resume_state;
    task->field_1C = record;
    task->field_20 = value;
    task->state_08 = 2;
}
