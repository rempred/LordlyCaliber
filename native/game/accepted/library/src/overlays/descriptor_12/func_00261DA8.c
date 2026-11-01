#include "game/effect_runtime_e03_internal.h"

/* The inline helper preserves KMC allocation and return-value lifetimes. */
static inline void *e03AllocatePool80(void)
{
    E03Pool80 *record;

    if (D_80220D50 != 0xFFFF) {
        record = &D_80220D58[D_80220D50];
        D_80220D50 = record->next_00;
    } else {
        for (;;) {
        }
    }
    record->field_10 = 0;
    record->field_0C = 0;
    record->field_08 = 0;
    record->field_14 = 0;
    record->field_18 = 1;
    return &record->field_04;
}

void *func_00261DA8(void *value)
{
    void *payload;

    payload = e03AllocatePool80();
    if (payload != 0) {
        *(s32 *)payload = (s32)value;
        func_001F0F6C((s32)payload);
    }
    return payload;
}
