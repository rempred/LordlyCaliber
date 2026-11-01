#include "game/effect_runtime_e03_internal.h"

void *func_00263074(void)
{
    E03RenderRecord *record;

    if (D_80220DA0 != 0xFFFF) {
        record = &D_80220DA8[D_80220DA0];
        D_80220DA0 = record->next_free_00;
    } else {
        for (;;) {
        }
    }
    record->field_10 = 2;
    record->field_0C = 0;
    record->field_08 = 0;
    return record;
}
