#include "game/effect_runtime_e03_internal.h"

void func_00261D7C(void *payload)
{
    E03Pool80 *record;

    record = (E03Pool80 *)((u8 *)payload - 4);
    record->next_00 = D_80220D50;
    D_80220D50 = ((s32)record - (s32)D_80220D58) >> 7;
}
