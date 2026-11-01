#include "game/effect_runtime_e03_internal.h"

void func_00261E90(void)
{
    E03Pool94 *record;
    s32 index;

    D_80220D68 = 32;
    D_80220D60 = func_80070F30(0x1300);
    D_80220D6C = D_80220D60;
    D_80220D70 = (E03Pool94 **)&D_80220D6C[D_80220D68];
    for (index = 0; index < D_80220D68; index++) {
        D_80220D6C[index].callback_04 = 0;
    }

    record = D_80220D6C;
    D_80220D64 = 0;
    for (index = 0; index < D_80220D68 - 1; index++, record++) {
        record->next_00 = index + 1;
    }
    record->next_00 = 0xFFFF;
}
