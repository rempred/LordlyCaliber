#include "game/effect_runtime_e03_internal.h"

void func_00261E30(void *payload)
{
    E03Pool80 *record;

    if (payload != 0) {
        func_001F0F90(payload);
        record = (E03Pool80 *)((u8 *)payload - 4);
        record->next_00 = D_80220D50;
        D_80220D50 = ((s32)record - (s32)D_80220D58) >> 7;
    }
}
