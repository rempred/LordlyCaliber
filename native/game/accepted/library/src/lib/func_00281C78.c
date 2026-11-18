#include "game/cutscene_panels.h"

extern s8 D_8022A998;
extern u8 D_800E7A32;

extern void func_00283664(void);
extern s32 func_00284288(void);
extern void func_00299740(void);
extern void func_0028B788(void);
extern void func_00290CF8(void);

void func_00281C78(s32 slot)
{
    func_00283664();
    func_00284288();

    if ((u8)D_8022A998 == 1) {
        u8 *flags = &D_800E7A32;
        u8 previous = *flags;

        D_8022A998 = 0;
        *flags = previous | 2;
    }

    func_00299740();
    func_0028B788();
    func_00290CF8();
}
