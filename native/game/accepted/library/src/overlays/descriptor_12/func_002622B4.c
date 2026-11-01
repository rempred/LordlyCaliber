#include "game/effect_runtime_e03_internal.h"

void func_002622B4(void *recordView)
{
    E03Pool94 *record = recordView;
    record->callback_04 = 0;
    record->next_00 = D_80220D64;
    D_80220D64 = record - D_80220D6C;
}
