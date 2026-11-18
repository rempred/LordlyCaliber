#include "game/cutscene_panels.h"

extern void *D_8018F1A4;
extern u16 D_800C4C26;

extern void func_00019e10(void);

void func_00281ED0(s32 slot)
{
    func_00019e10();
    func_0028b0e0();

    if (D_8018F1A4 != 0) {
        D_800C4C26 = 2;
    } else {
        D_800C4C26 = 0xFFFE;
    }
}
