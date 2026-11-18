#include "game/chapter_card.h"

extern u16 D_8018F1A0;
extern void bzero(void *buffer, s32 bytes);

void func_00289D24(void)
{
    ChapterCard *card = D_8023DE20;
    u16 selection = D_8018F1A0;
    u16 variant;
    s32 i = 2;
    s32 centerX = 160;
    TitleImageHeader *image = card->secondary[1];
    TitleImageHeader *mask;
    s32 y;
    s32 baseY;
    u16 height;
    u8 secondaryCount;

    variant = selection - 1073;
    card->revealSlot = 2;
    mask = card->images[0];
    height = image->height;
    secondaryCount = D_8023DE20->secondaryCount;
    baseY = 120 - (height >> 1);
    y = baseY - secondaryCount * 16;

    do {
        u8 slot = i;
        image = D_8023DE20->secondary[slot];
        ++i;
        if (image != 0) {
            ChapterCard *reveal;
            s32 rowBytes = func_0028ac88(image->field03, image->width);
            bzero(image->pixels, rowBytes * image->height);
            reveal = D_8023DE20;
            reveal->x[slot] = centerX - (image->width >> 1);
            reveal->y[slot] = y + (slot - 1) * 32 + 12;
            reveal->revealTicks[slot] = image->width + mask->width;
        }
    } while ((u8)i < 5);

    if (variant == 4) {
        ChapterCard *adjusted = D_8023DE20;
        adjusted->x[2] -= 30;
        adjusted->x[4] += 30;
    }
}
