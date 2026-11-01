#include "game/effect_runtime_e03_internal.h"

void func_00262DD4(u16 value)
{
    E03RenderRecord *temporary;
    s32 used;
    s32 index;
    s32 scan;
    s32 right;

    /* This initialization order retains the KMC save/init scheduling. */
    index = 0;
    used = 0;
    for (; index < D_80220DA4; index++) {
        if (D_80220DA8[index].callback_04 != 0 &&
            D_80220DA8[index].field_0C == 1 &&
            D_80220DA8[index].field_10 == value) {
            D_80220DAC[used] = &D_80220DA8[index];
            used++;
        }
    }
    if (used > 0) {
        for (index = 0, right = used - 1; index < right; index++, right--) {
            for (scan = index; scan <= right; scan++) {
                if (D_80220DAC[scan]->field_08 <
                    D_80220DAC[index]->field_08) {
                    temporary = D_80220DAC[index];
                    D_80220DAC[index] = D_80220DAC[scan];
                    D_80220DAC[scan] = temporary;
                }
                if (D_80220DAC[scan]->field_08 >
                    D_80220DAC[right]->field_08) {
                    temporary = D_80220DAC[right];
                    D_80220DAC[right] = D_80220DAC[scan];
                    D_80220DAC[scan] = temporary;
                }
            }
        }
        for (index = 0; index < used; index++) {
            D_80220DAC[index]->callback_04(D_80220DAC[index]);
        }
    }
}
