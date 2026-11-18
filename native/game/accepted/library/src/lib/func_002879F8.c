#include "game/chapter_card.h"

extern s32 resource_free(u8 *buffer);

void func_002879F8(void)
{
    resource_free((u8 *)D_8023E580);
    resource_free((u8 *)D_8023E588);
    resource_free(D_8023E584);

    D_8023E580 = 0;
    D_8023E588 = 0;
    D_8023E584 = 0;
}
