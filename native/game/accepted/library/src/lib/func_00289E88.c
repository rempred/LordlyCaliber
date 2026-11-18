#include "game/chapter_card.h"

s32 func_00289E88(void)
{
    s32 i = 2;
    ChapterCard *card = D_8023DE20;

    do {
        if (card->revealTicks[(u8)i] != 0) {
            return 1;
        }
        ++i;
    } while ((u8)i < 5);

    return 0;
}
