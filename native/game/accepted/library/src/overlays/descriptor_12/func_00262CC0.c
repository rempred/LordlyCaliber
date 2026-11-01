#include "game/effect_runtime_e03_internal.h"

void func_00262CC0(void)
{
    s32 index;

    D_80220DA4 = 256;
    D_80220DA8 = func_80070F30(0x7800);
    for (index = 0; index < D_80220DA4; index++) {
        D_80220DA8[index].callback_04 = 0;
        D_80220DA8[index].field_0C = 0;
    }
    D_80220DAC = func_80070F30(D_80220DA4 * 4);
    D_80220DA0 = 0;
    for (index = 0; index < D_80220DA4 - 1; index++) {
        D_80220DA8[index].next_free_00 = index + 1;
    }
    D_80220DA8[index].next_free_00 = 0xFFFF;
}
