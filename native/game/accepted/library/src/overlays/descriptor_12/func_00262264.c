#include "game/effect_runtime_e03_internal.h"

void *func_00262264(void)
{
    E03Pool94 *record;

    if (D_80220D64 != 0xFFFF) {
        record = &D_80220D6C[D_80220D64];
        D_80220D64 = record->next_00;
    } else {
        for (;;) {
        }
    }
    return record;
}
