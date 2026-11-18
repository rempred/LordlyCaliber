#include "game/chapter_card.h"

extern u8 D_800E7A32;
extern u8 D_800E7A33;
extern TitleSceneCallback D_800E7A44;
extern TitleSceneCallback D_800E7A48;
extern TitleSceneCallback D_800E7A4C;
extern u16 D_800E7A50;
extern void *D_800E7A54;
extern u8 D_800E91D0[];

extern void bzero(void *buffer, s32 size);

void func_00281F10(s32 slot)
{
    /* Snapshot both flag bytes before installing the callbacks. Keep this
     * store order for KMC's scheduling of the two independent flag writes.
     */
    u8 flags32 = D_800E7A32;
    u8 flags33 = D_800E7A33;

    D_800E7A44 = func_00281FA8;
    D_800E7A48 = func_00282004;
    D_800E7A4C = func_00282094;
    D_800E7A50 = 0x5C;
    D_800E7A54 = 0;
    D_800E7A32 = flags32 | 9;
    D_800E7A33 = flags33 | 1;
    bzero(D_800E91D0, 0x5C);
    func_0028A310();
}
