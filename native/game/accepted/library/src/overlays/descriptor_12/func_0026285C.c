#include "game/effect_runtime_e03_internal.h"

s32 func_0026285C(void *taskView, u16 command)
{
    E03Task *task = taskView;
    s32 result;

    D_80220FE8 = task;
    D_80220D94[--D_80220D90] = task;
    result = task->callback_04(command, task->payload_24);
    D_80220FE8 = D_80220D94[++D_80220D90];
    return result;
}
