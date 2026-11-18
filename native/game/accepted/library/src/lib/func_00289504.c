#include "game/chapter_card.h"

extern s32 resource_free(u8 *buffer);

void func_00289504(void)
{
    resource_free((u8 *)D_8023DE14);
    resource_free((u8 *)D_8023DE10);
    resource_free((u8 *)D_8023E594);
    resource_free((u8 *)D_8023E598);
    resource_free((u8 *)D_8023E59C);
}
