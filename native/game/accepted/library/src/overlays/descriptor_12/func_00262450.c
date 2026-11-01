#include "game/effect_runtime_e03_internal.h"

void func_00262450(void)
{
    s32 index;

    D_80220D80 = 24;
    D_80220E8E = 0;
    D_80220D8C = 0;
    D_80220D88 = 0;
    D_80220FE8 = 0;
    D_80220D90 = 24;
    D_80220EDC = func_80070F30(0xF04);
    D_80220D84 = D_80220EDC;
    D_80220D94 = (E03Task **)&D_80220D84[D_80220D80];
    D_80220D94[D_80220D90] = 0;
    for (index = 0; index < D_80220D80; index++) {
        D_80220D84[index].callback_04 = 0;
    }
    for (index = 0; index < D_80220D80 - 1; index++) {
        D_80220D84[index].next_free_00 = index + 1;
    }
    D_80220D84[index].next_free_00 = 0xFFFF;
}
