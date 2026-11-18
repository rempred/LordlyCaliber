#include "game/chapter_card.h"

extern void func_0028752C(TitleImageHeader *image, s32 startX, s32 startY,
                         s32 width, s32 height, s32 x, s32 y);

void func_00289558(void)
{
    TitleImageHeader *initialTens = D_8023DE10;
    TitleImageHeader *tens;
    u16 x = 0;
    s32 firstX;
    TitleImageHeader *label;
    u16 labelWidth;
    u16 onesWidth;
    CombatDrawCommand *command;
    s32 nextX;
    u16 width;

    if (initialTens != 0) {
        x = initialTens->width;
    }
    label = D_8023E598;
    labelWidth = label->width;
    onesWidth = D_8023DE14->width;

    command = gTitleCardDisplayListCursor;
    command[0].first = 0xDE000000;
    command[0].second = (u32)D_8022AA40;
    command[4].first = 0xFB000000;
    command[4].second = 0xFFFFFF00;
    command[1].first = 0xE3000C00;
    command[1].second = 0;
    command[2].first = 0xE200001C;
    command[2].second = 0x005041C8;
    command[3].first = 0xFCFFFFFF;
    command[3].second = 0xFFFEF379;
    command[5].first = 0xE7000000;
    command[5].second = 0;

    /* The volatile cursor preserves each advance of the six-command sequence. */
    gTitleCardDisplayListCursor = command + 1;
    gTitleCardDisplayListCursor = command + 2;
    gTitleCardDisplayListCursor = command + 3;
    gTitleCardDisplayListCursor = command + 4;
    gTitleCardDisplayListCursor = command + 5;
    gTitleCardDisplayListCursor = command + 6;

    /* Accumulate modulo 16 bits, then center with signed division. */
    x += labelWidth + onesWidth;
    firstX = (320 - x) / 2;
    func_0028752C(label, 0, 0, label->width, label->height, (u16)firstX, 60);

    nextX = firstX + D_8023E598->width;
    tens = D_8023DE10;
    x = nextX;
    if (tens != 0) {
        func_0028752C(tens, 0, 0, tens->width, tens->height, (u16)x, 61);
        x = D_8023DE10->width + nextX;
    }
    {
        TitleImageHeader *ones = D_8023DE14;

        func_0028752C(ones, 0, 0, ones->width, ones->height, (u16)x, 61);
    }
    {
        TitleImageHeader *title = D_8023E594;

        width = title->width;
        func_0028752C(title, 0, 0, width, title->height, (320 - width) / 2, 110);
    }
}
