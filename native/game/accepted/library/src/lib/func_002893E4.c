#include "game/chapter_card.h"

extern void *func_0002dfb8(void *buffer, u32 key);
extern void *func_0002e138(u32 key);
extern s32 resource_free(u8 *buffer);
extern u8 D_801936AB;

#define D_801936A7 (*(u8 *)0x801936A7)

void func_002893E4(void)
{
    u32 *keys;
    u8 sequence;
    u8 tens;
    TitleImageHeader *thirdImage;

    void *buffer = 0;
    u8 *counter = &D_801936AB;

    ++*counter;
    keys = func_0002dfb8(buffer, 0x01890C90);
    D_8023E594 = func_0002e138(keys[32 + D_801936A7]);
    D_8023E598 = func_0002e138(keys[30]);
    thirdImage = func_0002e138(keys[31]);
    sequence = *counter;
    D_8023E59C = thirdImage;
    tens = (u32)sequence / 10;
    if (tens != 0) {
        D_8023DE10 = func_0002e138(keys[20 + tens]);
    } else {
        D_8023DE10 = 0;
    }
    D_8023DE14 = func_0002e138(keys[20 + (u8)((u32)sequence % 10)]);
    resource_free((u8 *)keys);
}
