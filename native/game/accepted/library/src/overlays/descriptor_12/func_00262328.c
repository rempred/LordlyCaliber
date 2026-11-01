#include "game/effect_runtime_e03_internal.h"

void *func_00262328(void *callback)
{
    E03Pool94 *record;

    if (D_80220D64 != 0xFFFF) {
        record = &D_80220D6C[D_80220D64];
        D_80220D64 = record->next_00;
    } else {
        for (;;) {
        }
    }
    if (record != 0) {
        record->flags_08 = 7;
        record->callback_04 = 0;
        record->flags_0A = 0;
        record->field_0C = 0x708;
        record->field_10 = 0;
        record->callback_04 = (E03Callback94)callback;
        ((E03Callback94)callback)(0, record);
    }
    return record;
}
