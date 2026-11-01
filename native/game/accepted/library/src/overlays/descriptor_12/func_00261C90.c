#include "game/effect_runtime_e03_internal.h"

void func_00261C90(void)
{
    E03Pool80 *slot;
    u32 last;
    u32 index;

    D_80220D54 = 0x100;
    slot = func_80070F30(0x8000);
    last = D_80220D54 - 1;
    D_80220D58 = slot;
    D_80220D50 = 0;

    for (index = 0; index < last; index++, slot++) {
        slot->next_00 = index + 1;
    }
    slot->next_00 = 0xFFFF;
}
