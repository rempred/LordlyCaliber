#include "game/effect_runtime_e03_internal.h"

void func_002625AC(void)
{
    E03Task *task;

    task = D_80220D88;
    D_80220FE8 = 0;
    while (task != 0) {
        if (task->state_08 >= 0x100) {
            func_0026285C(task, task->state_08);
        } else {
            switch (task->state_08) {
            case 1:
                if (task->countdown_1A != -1) {
                    task->countdown_1A--;
                    if (task->countdown_1A <= 0) {
                        task->state_08 = task->resume_state_18;
                    }
                }
                break;
            case 2:
                if (task->field_1C->field_10 >= task->field_20) {
                    task->state_08 = task->resume_state_18;
                }
                break;
            }
        }
        task = task->next_10;
    }
    D_80220FE8 = 0;
}
