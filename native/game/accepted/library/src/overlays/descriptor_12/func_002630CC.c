#include "game/effect_runtime_e03_internal.h"

void func_002630CC(void *recordView)
{
    E03RenderRecord *record = recordView;
    if (record != 0) {
        record->callback_04 = 0;
        record->next_free_00 = D_80220DA0;
        D_80220DA0 = record - D_80220DA8;
    }
}
