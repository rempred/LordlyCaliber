#include "game/effect_runtime_e03_internal.h"

/* Includes the physical 00261D60 continuation in the same callable body. */
void *func_00261D24(void)
{
    E03Pool80 *record;

    if (D_80220D50 != 0xFFFF) {
        record = &D_80220D58[D_80220D50];
        D_80220D50 = record->next_00;
    } else {
        /* The original pool-exhaustion branch spins at ROM 00261D58. */
        for (;;) {
        }
    }
    record->field_18 = 1;
    record->field_10 = 0;
    record->field_0C = 0;
    record->field_08 = 0;
    record->field_14 = 0;
    return &record->field_04;
}
